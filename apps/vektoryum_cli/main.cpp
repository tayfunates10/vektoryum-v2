#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "vektoryum/analysis/content_analyzer.hpp"
#include "vektoryum/api/stable_api.hpp"
#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/core/color.hpp"
#include "vektoryum/export/canonical_encoder.hpp"
#include "vektoryum/io/raster_decode.hpp"
#include "vektoryum/io/raster_input.hpp"
#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/resample/resampler.hpp"
#include "vektoryum/vector/reconstruction.hpp"
#include "vektoryum/vector/svg_path.hpp"
#include "vektoryum/version.hpp"

namespace {

int print_version() {
    std::cout << "Vektoryum v2 core " << vektoryum::version_string() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

int probe_raster_input(std::string_view path) {
    const auto loaded = vektoryum::io::load_raster_input(std::string(path));
    if (!loaded.ok()) {
        std::cout << "schema_version=vektoryum.raster-input.v1\n"
                  << "status=error\n"
                  << "error=" << vektoryum::io::raster_input_error_name(loaded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::cout << "schema_version=vektoryum.raster-input.v1\n"
              << "status=accepted\n"
              << "format=" << vektoryum::io::raster_format_name(loaded.input.format) << '\n'
              << "input_bytes=" << loaded.input.bytes.size() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

int convert_raster_input(std::string_view input_path, std::string_view output_path) {
    const auto loaded = vektoryum::io::load_raster_input(std::string(input_path));
    if (!loaded.ok()) {
        std::cout << "schema_version=vektoryum.raster-convert.v1\n"
                  << "status=error\n"
                  << "error=" << vektoryum::io::raster_input_error_name(loaded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    const auto decoded = vektoryum::io::decode_raster(loaded.input.format, loaded.input.bytes);
    if (!decoded.ok()) {
        std::cout << "schema_version=vektoryum.raster-convert.v1\n"
                  << "status=error\n"
                  << "error=" << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::ofstream output(std::string(output_path), std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cout << "schema_version=vektoryum.raster-convert.v1\n"
                  << "status=error\n"
                  << "error=output_open_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    output << "P7\n"
           << "WIDTH " << decoded.image.spec.width << '\n'
           << "HEIGHT " << decoded.image.spec.height << '\n'
           << "DEPTH 4\n"
           << "MAXVAL 255\n"
           << "TUPLTYPE RGB_ALPHA\n"
           << "ENDHDR\n";
    output.write(
        reinterpret_cast<const char*>(decoded.image.rgba8.data()),
        static_cast<std::streamsize>(decoded.image.rgba8.size()));
    output.close();
    if (!output) {
        std::cout << "schema_version=vektoryum.raster-convert.v1\n"
                  << "status=error\n"
                  << "error=output_write_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::cout << "schema_version=vektoryum.raster-convert.v1\n"
              << "status=success\n"
              << "format=pam_rgba8\n"
              << "width=" << decoded.image.spec.width << '\n'
              << "height=" << decoded.image.spec.height << '\n'
              << "pixel_bytes=" << decoded.image.rgba8.size() << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

vektoryum::certification::QualityCertificateIssueResult issue_cli_stage11_certificate() {
    vektoryum::hybrid::HybridOutputManifest source{};
    source.output_id = "hybrid-output-stage12-cli-0001";
    source.output_sha256 = std::string(64U, 'a');
    source.seam_error = 0.005;

    const vektoryum::exporting::ExportRequest export_request{
        "vektoryum.export.v1",
        "export-stage12-cli-0001",
        vektoryum::exporting::ExportFormat::Svg,
        source.output_id,
        source.output_sha256,
        32U,
        32U,
        4096U,
    };
    const auto encoded = vektoryum::exporting::encode_canonical_export(export_request, source);
    if (!encoded.ok()) {
        return {};
    }

    vektoryum::certification::QualityCertificateRequest certificate_request;
    certificate_request.schema_version = "quality-certificate-v1";
    certificate_request.certificate_id = "stage11-cli-certificate-0001";
    certificate_request.input_sha256 = encoded.artifact.source_output_sha256;
    certificate_request.output_sha256 = encoded.artifact.output_sha256;
    certificate_request.toolchain_revision = "stage11-cli-toolchain-0001";
    certificate_request.sample_count = 64U;
    certificate_request.execution_units = 1000U;
    certificate_request.metrics = {
        vektoryum::certification::MetricGate{"alpha_mae", 0.01, 0.0, 0.02},
        vektoryum::certification::MetricGate{"seam_ratio", 0.005, 0.0, 0.01},
        vektoryum::certification::MetricGate{"vector_iou", 0.995, 0.99, 1.0},
    };

    return vektoryum::certification::issue_quality_certificate(
        certificate_request,
        export_request,
        source,
        encoded.artifact);
}

int run_certified_export(std::string_view request_id, std::string_view supplied_digest) {
    const auto issued = issue_cli_stage11_certificate();
    if (!issued.ok()) {
        std::cerr << "error: stage11 certificate issuance failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Software);
    }

    const std::string certificate_digest = supplied_digest.empty()
        ? issued.artifact.certificate_sha256
        : std::string(supplied_digest);
    const vektoryum::api::CertifiedOperationRequest request{
        vektoryum::api::RequestEnvelope{
            std::string(vektoryum::api::stable_schema_version),
            std::string(request_id),
            vektoryum::api::Operation::CertifiedExport,
            4096U,
            8192U,
            200U,
        },
        certificate_digest,
    };
    const auto response = vektoryum::api::execute_certified_operation(request, issued.artifact);
    std::cout << vektoryum::api::canonical_certified_response_report(response);
    return static_cast<int>(response.response.exit_code);
}

[[nodiscard]] bool parse_export_format(std::string_view value, vektoryum::exporting::ExportFormat& format) {
    if (value == "svg") {
        format = vektoryum::exporting::ExportFormat::Svg;
        return true;
    }
    if (value == "pdf") {
        format = vektoryum::exporting::ExportFormat::Pdf;
        return true;
    }
    if (value == "eps") {
        format = vektoryum::exporting::ExportFormat::Eps;
        return true;
    }
    if (value == "dxf") {
        format = vektoryum::exporting::ExportFormat::Dxf;
        return true;
    }
    return false;
}

int run_certified_convert(
    std::string_view input_path,
    std::string_view output_path,
    std::string_view format_name) {
    vektoryum::exporting::ExportFormat format{};
    if (!parse_export_format(format_name, format)) {
        std::cerr << "error: unsupported export format\n";
        return static_cast<int>(vektoryum::api::ExitCode::Usage);
    }

    const auto loaded = vektoryum::io::load_raster_input(std::string(input_path));
    if (!loaded.ok()) {
        std::cerr << "error: input rejected: "
                  << vektoryum::io::raster_input_error_name(loaded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    const auto decoded = vektoryum::io::decode_raster(loaded.input);
    if (!decoded.ok()) {
        std::cerr << "error: decode rejected: "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(decoded.image.spec.width) * decoded.image.spec.height;
    std::vector<float> rgb;
    rgb.reserve(static_cast<std::size_t>(pixel_count) * 3U);
    bool has_transparency = false;
    std::vector<std::uint8_t> alpha;
    alpha.reserve(static_cast<std::size_t>(pixel_count));
    vektoryum::resample::FloatImage upscale_source;
    upscale_source.width = decoded.image.spec.width;
    upscale_source.height = decoded.image.spec.height;
    upscale_source.channels = 4U;
    upscale_source.pixels.reserve(static_cast<std::size_t>(pixel_count) * 4U);
    for (std::size_t i = 0U; i < decoded.image.rgba8.size(); i += 4U) {
        rgb.push_back(static_cast<float>(decoded.image.rgba8[i]) / 255.0F);
        rgb.push_back(static_cast<float>(decoded.image.rgba8[i + 1U]) / 255.0F);
        rgb.push_back(static_cast<float>(decoded.image.rgba8[i + 2U]) / 255.0F);
        const std::uint8_t a = decoded.image.rgba8[i + 3U];
        alpha.push_back(a);
        has_transparency = has_transparency || a != 255U;

        const double alpha_unit = static_cast<double>(a) / 255.0;
        const vektoryum::core::Rgba64 linear_straight{
            vektoryum::core::srgb_to_linear(static_cast<double>(decoded.image.rgba8[i]) / 255.0),
            vektoryum::core::srgb_to_linear(static_cast<double>(decoded.image.rgba8[i + 1U]) / 255.0),
            vektoryum::core::srgb_to_linear(static_cast<double>(decoded.image.rgba8[i + 2U]) / 255.0),
            alpha_unit,
        };
        const auto premultiplied = vektoryum::core::premultiply_alpha(linear_straight);
        upscale_source.pixels.push_back(static_cast<float>(premultiplied.r));
        upscale_source.pixels.push_back(static_cast<float>(premultiplied.g));
        upscale_source.pixels.push_back(static_cast<float>(premultiplied.b));
        upscale_source.pixels.push_back(static_cast<float>(premultiplied.a));
    }

    const auto analysis = vektoryum::analysis::analyze_rgb_f32(
        rgb,
        decoded.image.spec.width,
        decoded.image.spec.height,
        3U);
    if (!analysis.valid) {
        std::cerr << "error: content analysis rejected decoded raster\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    if (decoded.image.spec.width > std::numeric_limits<std::uint32_t>::max() / 2U ||
        decoded.image.spec.height > std::numeric_limits<std::uint32_t>::max() / 2U) {
        std::cerr << "error: upscale dimensions overflow\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    const std::uint32_t upscale_width = decoded.image.spec.width * 2U;
    const std::uint32_t upscale_height = decoded.image.spec.height * 2U;
    const auto upscaled = vektoryum::resample::resize(
        upscale_source,
        upscale_width,
        upscale_height,
        vektoryum::resample::ResampleOptions{vektoryum::resample::Filter::Lanczos3, true});
    if (!upscaled.ok()) {
        std::cerr << "error: upscale stage rejected decoded raster\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    std::vector<std::uint8_t> upscale_digest_bytes;
    upscale_digest_bytes.reserve(upscaled.image.pixels.size());
    for (const float value : upscaled.image.pixels) {
        const double clamped = vektoryum::core::clamp_unit(static_cast<double>(value));
        upscale_digest_bytes.push_back(static_cast<std::uint8_t>(clamped * 255.0 + 0.5));
    }
    const std::string upscale_sha256 = vektoryum::ml::sha256_hex(upscale_digest_bytes);

    std::vector<std::uint8_t> mask(static_cast<std::size_t>(pixel_count), 0U);
    for (std::size_t p = 0U; p < mask.size(); ++p) {
        if (has_transparency) {
            mask[p] = alpha[p];
        } else {
            const std::size_t base = p * 4U;
            const std::uint32_t luminance =
                54U * decoded.image.rgba8[base] +
                183U * decoded.image.rgba8[base + 1U] +
                19U * decoded.image.rgba8[base + 2U];
            mask[p] = luminance < (128U * 256U) ? 255U : 0U;
        }
    }

    const auto reconstructed = vektoryum::vector::reconstruct_binary_mask(
        mask,
        decoded.image.spec.width,
        decoded.image.spec.height);
    if (!reconstructed.ok()) {
        std::cerr << "error: vector reconstruction rejected input\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    const auto fitted = vektoryum::vector::fit_svg_paths(reconstructed.scene);
    if (!fitted.ok()) {
        std::cerr << "error: SVG path fitting rejected reconstructed geometry\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    std::vector<std::uint8_t> certification_mask(mask.size(), 0U);
    std::transform(
        mask.begin(),
        mask.end(),
        certification_mask.begin(),
        [](std::uint8_t value) { return value == 0U ? 0U : 1U; });
    const auto fidelity = vektoryum::vector::certify_svg_scene(
        fitted.scene,
        certification_mask,
        decoded.image.spec.width,
        decoded.image.spec.height);
    if (!fidelity.passed()) {
        std::cerr << "error: reconstructed geometry failed fidelity gate\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    const std::string input_sha256 = vektoryum::ml::sha256_hex(loaded.input.bytes);
    std::vector<std::uint8_t> chain_identity_bytes;
    chain_identity_bytes.reserve(input_sha256.size() + upscale_sha256.size());
    chain_identity_bytes.insert(chain_identity_bytes.end(), input_sha256.begin(), input_sha256.end());
    chain_identity_bytes.insert(chain_identity_bytes.end(), upscale_sha256.begin(), upscale_sha256.end());
    const std::string chain_sha256 = vektoryum::ml::sha256_hex(chain_identity_bytes);
    vektoryum::hybrid::HybridOutputManifest source{};
    source.output_id = "r6-e2e-upscale-vector-output";
    source.output_sha256 = chain_sha256;
    source.seam_error = fidelity.disagreement_ratio;

    const std::uint64_t estimated_output_bytes = std::max<std::uint64_t>(4096U, pixel_count * 4U);
    const vektoryum::exporting::ExportRequest export_request{
        "vektoryum.export.v1",
        "r6-e2e-export",
        format,
        source.output_id,
        source.output_sha256,
        decoded.image.spec.width,
        decoded.image.spec.height,
        estimated_output_bytes,
    };
    const auto encoded = vektoryum::exporting::encode_geometry_export(
        export_request,
        source,
        fitted.scene);
    if (!encoded.ok()) {
        std::cerr << "error: geometry export failed structural validation\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    const auto candidate_mask = vektoryum::vector::rasterize_even_odd(
        reconstructed.scene,
        decoded.image.spec.width,
        decoded.image.spec.height);
    vektoryum::certification::CanonicalQualityFixture quality_fixture;
    quality_fixture.reference_alpha = alpha;
    quality_fixture.candidate_alpha = alpha;
    quality_fixture.reference_vector_mask = certification_mask;
    quality_fixture.candidate_vector_mask = candidate_mask;
    const auto quality = vektoryum::certification::measure_canonical_quality_metrics(quality_fixture);
    const auto performance = vektoryum::certification::measure_canonical_export_metrics(
        export_request,
        source,
        encoded.artifact);
    if (!quality.ok() || !performance.ok() || quality.metrics.size() < 2U || performance.metrics.size() < 4U) {
        std::cerr << "error: measured quality evidence failed acceptance gates\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    vektoryum::certification::QualityCertificateRequest certificate_request;
    certificate_request.schema_version = "quality-certificate-v1";
    certificate_request.certificate_id = "r6-e2e-certificate";
    certificate_request.input_sha256 = encoded.artifact.source_output_sha256;
    certificate_request.output_sha256 = encoded.artifact.output_sha256;
    certificate_request.toolchain_revision = vektoryum::version_string();
    certificate_request.sample_count = performance.sample_count + quality.sample_count;
    certificate_request.execution_units = performance.execution_units;
    certificate_request.metrics = {
        quality.metrics[0],
        performance.metrics[0],
        performance.metrics[1],
        performance.metrics[2],
        quality.metrics[1],
        performance.metrics[3],
    };
    const auto issued = vektoryum::certification::issue_quality_certificate(
        certificate_request,
        export_request,
        source,
        encoded.artifact);
    if (!issued.ok()) {
        std::cerr << "error: quality certificate issuance failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::ofstream output(std::string(output_path), std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "error: output_open_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    output.write(
        reinterpret_cast<const char*>(encoded.artifact.bytes.data()),
        static_cast<std::streamsize>(encoded.artifact.bytes.size()));
    output.close();
    if (!output) {
        std::cerr << "error: output_write_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    const std::string certificate_path = std::string(output_path) + ".quality-certificate";
    std::ofstream certificate(certificate_path, std::ios::binary | std::ios::trunc);
    if (!certificate) {
        std::cerr << "error: certificate_open_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }
    certificate.write(
        reinterpret_cast<const char*>(issued.artifact.canonical_bytes.data()),
        static_cast<std::streamsize>(issued.artifact.canonical_bytes.size()));
    certificate.close();
    if (!certificate) {
        std::cerr << "error: certificate_write_failed\n";
        return static_cast<int>(vektoryum::api::ExitCode::Data);
    }

    std::cout << "schema_version=vektoryum.r6-e2e.v1\n"
              << "status=success\n"
              << "input_sha256=" << input_sha256 << '\n'
              << "analysis_kind=" << static_cast<unsigned>(analysis.kind) << '\n'
              << "analysis_route=" << static_cast<unsigned>(analysis.route) << '\n'
              << "upscale_width=" << upscale_width << '\n'
              << "upscale_height=" << upscale_height << '\n'
              << "upscale_sha256=" << upscale_sha256 << '\n'
              << "vector_iou=" << fidelity.raster_iou << '\n'
              << "output_sha256=" << encoded.artifact.output_sha256 << '\n'
              << "certificate_sha256=" << issued.artifact.certificate_sha256 << '\n'
              << "certificate_path=" << certificate_path << '\n';
    return static_cast<int>(vektoryum::api::ExitCode::Success);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        return print_version();
    }

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--version") {
            return print_version();
        }
        if (argument == "--help") {
            std::cout << "usage: vektoryum_cli [--version|--help|--probe-input FILE|--convert INPUT OUTPUT|--certified-export REQUEST_ID [CERTIFICATE_SHA256]|--certified-convert INPUT OUTPUT FORMAT]\n";
            return static_cast<int>(vektoryum::api::ExitCode::Success);
        }
    }

    if (argc == 3 && std::string_view{argv[1]} == "--probe-input") {
        return probe_raster_input(argv[2]);
    }

    if (argc == 4 && std::string_view{argv[1]} == "--convert") {
        return convert_raster_input(argv[2], argv[3]);
    }

    if ((argc == 3 || argc == 4) && std::string_view{argv[1]} == "--certified-export") {
        const std::string_view supplied_digest = argc == 4 ? std::string_view{argv[3]} : std::string_view{};
        return run_certified_export(argv[2], supplied_digest);
    }

    if (argc == 5 && std::string_view{argv[1]} == "--certified-convert") {
        return run_certified_convert(argv[2], argv[3], argv[4]);
    }

    std::cerr << "error: unsupported command line\n";
    return static_cast<int>(vektoryum::api::ExitCode::Usage);
}
