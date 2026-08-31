#include "vektoryum/io/raster_decode.hpp"

#include "tiff_baseline_fixtures.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] std::uint8_t byte_of(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value & 0xffU);
}

[[nodiscard]] std::vector<std::uint8_t> render(
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>& pixel) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::array<std::uint8_t, 4U> value = pixel(x, y);
            rgba.insert(rgba.end(), value.begin(), value.end());
        }
    }
    return rgba;
}

void check_fixture(
    std::string_view name,
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>& pixel) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, encoded);
    if (!decoded.ok()) {
        std::cerr << "FAIL: " << name << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    if (decoded.image.spec.width != width || decoded.image.spec.height != height) {
        std::cerr << "FAIL: " << name << " dimensions must match the IFD\n";
        ++failures;
        return;
    }
    expect(decoded.image.spec.layout == vektoryum::core::PixelLayout::RGBA,
           "TIFF must normalize to the canonical RGBA layout");
    expect(decoded.image.spec.transfer == vektoryum::core::TransferFunction::SRGB,
           "TIFF must normalize to the sRGB transfer function");
    expect(decoded.image.spec.alpha == vektoryum::core::AlphaMode::Straight,
           "TIFF must normalize to straight alpha");
    if (decoded.image.rgba8 != render(width, height, pixel)) {
        std::cerr << "FAIL: " << name << " pixels differ from the encoded source\n";
        ++failures;
    }
}

// A hand-built uncompressed TIFF, so the test can vary the parts a reference
// encoder does not expose: byte order, strip layout and the tags it omits.
struct TiffPlan {
    bool big_endian{false};
    std::uint32_t width{4U};
    std::uint32_t height{4U};
    std::uint32_t samples{3U};
    std::uint16_t photometric{2U};
    std::uint32_t rows_per_strip{4U};
    bool emit_samples_per_pixel{true};
    std::uint16_t extra_sample{0U};   // 0 omits the tag, 1 associated, 2 unassociated
    std::uint16_t compression{1U};
    std::uint16_t bits_per_sample{8U};
    std::uint32_t declared_strip_bytes{0U};   // 0 uses the correct value
};

class TiffBuilder {
public:
    explicit TiffBuilder(bool big_endian) noexcept : big_endian_(big_endian) {}

    void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) const {
        if (big_endian_) {
            out.push_back(static_cast<std::uint8_t>(value >> 8U));
            out.push_back(static_cast<std::uint8_t>(value & 0xffU));
        } else {
            out.push_back(static_cast<std::uint8_t>(value & 0xffU));
            out.push_back(static_cast<std::uint8_t>(value >> 8U));
        }
    }

    void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) const {
        if (big_endian_) {
            out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
            out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
            out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
            out.push_back(static_cast<std::uint8_t>(value & 0xffU));
        } else {
            out.push_back(static_cast<std::uint8_t>(value & 0xffU));
            out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
            out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
            out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
        }
    }

private:
    bool big_endian_;
};

// Samples are generated the same way the expectation is, so the fixture and the
// expected pixels cannot drift apart.
[[nodiscard]] std::uint8_t plan_sample(std::uint32_t x, std::uint32_t y, std::uint32_t channel) noexcept {
    return byte_of(x * 17U + y * 29U + channel * 53U + 7U);
}

[[nodiscard]] std::vector<std::uint8_t> build_tiff(const TiffPlan& plan) {
    const TiffBuilder writer(plan.big_endian);
    const std::size_t row_bytes = static_cast<std::size_t>(plan.width) * plan.samples;
    const std::size_t strip_count =
        (plan.height + plan.rows_per_strip - 1U) / plan.rows_per_strip;

    struct Entry {
        std::uint16_t tag;
        std::uint16_t type;   // 3 = SHORT, 4 = LONG
        std::uint32_t count;
        std::vector<std::uint32_t> values;
    };
    std::vector<Entry> entries;
    entries.push_back({256U, 4U, 1U, {plan.width}});
    entries.push_back({257U, 4U, 1U, {plan.height}});
    entries.push_back({258U, 3U, plan.samples, std::vector<std::uint32_t>(plan.samples, plan.bits_per_sample)});
    entries.push_back({259U, 3U, 1U, {plan.compression}});
    entries.push_back({262U, 3U, 1U, {plan.photometric}});
    entries.push_back({273U, 4U, static_cast<std::uint32_t>(strip_count), std::vector<std::uint32_t>(strip_count, 0U)});
    if (plan.emit_samples_per_pixel) {
        entries.push_back({277U, 3U, 1U, {plan.samples}});
    }
    entries.push_back({278U, 4U, 1U, {plan.rows_per_strip}});
    entries.push_back({279U, 4U, static_cast<std::uint32_t>(strip_count), std::vector<std::uint32_t>(strip_count, 0U)});
    if (plan.extra_sample != 0U) {
        entries.push_back({338U, 3U, 1U, {plan.extra_sample}});
    }

    std::vector<std::uint32_t> strip_offsets(strip_count, 0U);
    std::vector<std::uint32_t> strip_bytes(strip_count, 0U);
    for (std::size_t strip = 0U; strip < strip_count; ++strip) {
        const std::size_t rows =
            std::min<std::size_t>(plan.rows_per_strip, plan.height - strip * plan.rows_per_strip);
        strip_bytes[strip] = plan.declared_strip_bytes != 0U
                                 ? plan.declared_strip_bytes
                                 : static_cast<std::uint32_t>(rows * row_bytes);
    }

    const std::size_t ifd_size = 2U + entries.size() * 12U + 4U;
    std::size_t out_of_line = 8U + ifd_size;
    // Reserve the out-of-line area for the values that do not fit inline.
    for (Entry& entry : entries) {
        const std::size_t element = entry.type == 3U ? 2U : 4U;
        const std::size_t total = element * entry.count;
        if (total > 4U) {
            out_of_line += total;
        }
    }
    std::size_t pixel_base = out_of_line;
    for (std::size_t strip = 0U; strip < strip_count; ++strip) {
        const std::size_t rows =
            std::min<std::size_t>(plan.rows_per_strip, plan.height - strip * plan.rows_per_strip);
        strip_offsets[strip] = static_cast<std::uint32_t>(pixel_base);
        pixel_base += rows * row_bytes;
    }
    for (Entry& entry : entries) {
        if (entry.tag == 273U) {
            entry.values = strip_offsets;
        } else if (entry.tag == 279U) {
            entry.values = strip_bytes;
        }
    }

    std::vector<std::uint8_t> header;
    if (plan.big_endian) {
        header.insert(header.end(), {0x4dU, 0x4dU, 0x00U, 0x2aU});
    } else {
        header.insert(header.end(), {0x49U, 0x49U, 0x2aU, 0x00U});
    }
    writer.put_u32(header, 8U);

    std::vector<std::uint8_t> ifd;
    std::vector<std::uint8_t> spill;
    writer.put_u16(ifd, static_cast<std::uint16_t>(entries.size()));
    std::size_t spill_offset = 8U + ifd_size;
    for (const Entry& entry : entries) {
        writer.put_u16(ifd, entry.tag);
        writer.put_u16(ifd, entry.type);
        writer.put_u32(ifd, entry.count);
        const std::size_t element = entry.type == 3U ? 2U : 4U;
        const std::size_t total = element * entry.count;
        if (total <= 4U) {
            std::vector<std::uint8_t> inlined;
            for (const std::uint32_t value : entry.values) {
                if (entry.type == 3U) {
                    writer.put_u16(inlined, static_cast<std::uint16_t>(value));
                } else {
                    writer.put_u32(inlined, value);
                }
            }
            inlined.resize(4U, 0U);
            ifd.insert(ifd.end(), inlined.begin(), inlined.end());
        } else {
            writer.put_u32(ifd, static_cast<std::uint32_t>(spill_offset));
            for (const std::uint32_t value : entry.values) {
                if (entry.type == 3U) {
                    writer.put_u16(spill, static_cast<std::uint16_t>(value));
                } else {
                    writer.put_u32(spill, value);
                }
            }
            spill_offset += total;
        }
    }
    writer.put_u32(ifd, 0U);

    std::vector<std::uint8_t> pixels;
    for (std::uint32_t y = 0U; y < plan.height; ++y) {
        for (std::uint32_t x = 0U; x < plan.width; ++x) {
            for (std::uint32_t channel = 0U; channel < plan.samples; ++channel) {
                pixels.push_back(plan_sample(x, y, channel));
            }
        }
    }

    std::vector<std::uint8_t> tiff;
    tiff.insert(tiff.end(), header.begin(), header.end());
    tiff.insert(tiff.end(), ifd.begin(), ifd.end());
    tiff.insert(tiff.end(), spill.begin(), spill.end());
    tiff.insert(tiff.end(), pixels.begin(), pixels.end());
    return tiff;
}

void check_plan(std::string_view name, const TiffPlan& plan) {
    const auto bytes = build_tiff(plan);
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, bytes);
    if (!decoded.ok()) {
        std::cerr << "FAIL: " << name << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    const auto expected = render(plan.width, plan.height, [&plan](std::uint32_t x, std::uint32_t y) {
        if (plan.samples == 1U) {
            const std::uint8_t v = plan_sample(x, y, 0U);
            return std::array<std::uint8_t, 4U>{v, v, v, 255U};
        }
        return std::array<std::uint8_t, 4U>{
            plan_sample(x, y, 0U), plan_sample(x, y, 1U), plan_sample(x, y, 2U),
            plan.samples == 4U ? plan_sample(x, y, 3U) : static_cast<std::uint8_t>(255U)};
    });
    if (decoded.image.rgba8 != expected) {
        std::cerr << "FAIL: " << name << " pixels differ from the written samples\n";
        ++failures;
    }
}

void check_plan_rejected(std::string_view name, const TiffPlan& plan) {
    const auto bytes = build_tiff(plan);
    if (vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, bytes).ok()) {
        std::cerr << "FAIL: " << name << " must fail closed\n";
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace vektoryum_test_fixtures;

    check_fixture("greyscale 13x9", tiff_gray_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        const std::uint8_t v = byte_of(x * 11U + y * 7U);
        return std::array<std::uint8_t, 4U>{v, v, v, 255U};
    });
    check_fixture("RGB 13x9", tiff_rgb_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), 255U};
    });
    check_fixture("RGBA 13x9", tiff_rgba_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), byte_of((x * 255U) / 12U)};
    });

    // Both byte orders, every supported photometric layout, and strip layouts a
    // reference encoder will not produce on demand.
    for (const bool big_endian : {false, true}) {
        const std::string order = big_endian ? "big-endian " : "little-endian ";
        TiffPlan gray{};
        gray.big_endian = big_endian;
        gray.width = 5U;
        gray.height = 4U;
        gray.samples = 1U;
        gray.photometric = 1U;
        gray.rows_per_strip = 4U;
        check_plan(order + "greyscale", gray);

        TiffPlan gray_default_tag = gray;
        gray_default_tag.emit_samples_per_pixel = false;
        check_plan(order + "greyscale without SamplesPerPixel", gray_default_tag);

        TiffPlan rgb{};
        rgb.big_endian = big_endian;
        rgb.width = 5U;
        rgb.height = 4U;
        rgb.samples = 3U;
        rgb.rows_per_strip = 4U;
        check_plan(order + "RGB", rgb);

        TiffPlan rgba = rgb;
        rgba.samples = 4U;
        rgba.extra_sample = 2U;
        check_plan(order + "RGBA with unassociated alpha", rgba);

        TiffPlan striped = rgb;
        striped.height = 9U;
        striped.rows_per_strip = 2U;   // five strips, the last one short
        check_plan(order + "RGB in five strips", striped);

        TiffPlan single_row_strips = rgba;
        single_row_strips.height = 6U;
        single_row_strips.rows_per_strip = 1U;
        check_plan(order + "RGBA in one strip per row", single_row_strips);
    }

    // Associated alpha is un-premultiplied on the way to canonical straight alpha.
    {
        TiffPlan associated{};
        associated.width = 5U;
        associated.height = 4U;
        associated.samples = 4U;
        associated.extra_sample = 1U;
        associated.rows_per_strip = 4U;
        const auto bytes = build_tiff(associated);
        const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, bytes);
        expect(decoded.ok(), "associated-alpha TIFF must decode");
        if (decoded.ok()) {
            const std::uint8_t alpha = plan_sample(0U, 0U, 3U);
            expect(decoded.image.rgba8[3U] == alpha, "associated-alpha TIFF must preserve alpha");
            expect(decoded.image.rgba8[0U] >= plan_sample(0U, 0U, 0U),
                   "un-premultiplying must not darken a partially transparent sample");
            expect(decoded.image.spec.alpha == vektoryum::core::AlphaMode::Straight,
                   "associated alpha must be normalized to straight alpha");
        }
    }

    // Structural faults must fail closed rather than reading past a strip.
    {
        TiffPlan wrong_counts{};
        wrong_counts.width = 5U;
        wrong_counts.height = 4U;
        wrong_counts.rows_per_strip = 4U;
        wrong_counts.declared_strip_bytes = 7U;
        check_plan_rejected("TIFF with a StripByteCounts that does not match its rows", wrong_counts);

        TiffPlan compressed{};
        compressed.width = 5U;
        compressed.height = 4U;
        compressed.rows_per_strip = 4U;
        compressed.compression = 5U;   // LZW
        check_plan_rejected("compressed TIFF", compressed);

        TiffPlan deep{};
        deep.width = 5U;
        deep.height = 4U;
        deep.rows_per_strip = 4U;
        deep.bits_per_sample = 16U;
        check_plan_rejected("16-bit TIFF", deep);

        TiffPlan bad_photometric{};
        bad_photometric.width = 5U;
        bad_photometric.height = 4U;
        bad_photometric.rows_per_strip = 4U;
        bad_photometric.photometric = 5U;   // separated (CMYK)
        check_plan_rejected("CMYK TIFF", bad_photometric);

        TiffPlan unknown_extra{};
        unknown_extra.width = 5U;
        unknown_extra.height = 4U;
        unknown_extra.samples = 4U;
        unknown_extra.rows_per_strip = 4U;
        unknown_extra.extra_sample = 3U;
        check_plan_rejected("TIFF with an unknown ExtraSamples value", unknown_extra);
    }
    {
        TiffPlan plan{};
        plan.width = 5U;
        plan.height = 4U;
        plan.rows_per_strip = 4U;
        auto truncated = build_tiff(plan);
        truncated.resize(truncated.size() - 5U);
        expect(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, truncated).ok(),
               "a TIFF whose last strip runs past the file must fail closed");
    }
    {
        std::vector<std::uint8_t> stub{0x49U, 0x49U, 0x2aU, 0x00U, 0x08U, 0x00U, 0x00U, 0x00U};
        expect(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Tiff, stub).ok(),
               "a TIFF header with no IFD must fail closed");
    }

    return failures == 0 ? 0 : 1;
}
