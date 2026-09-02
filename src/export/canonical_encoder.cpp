#include "vektoryum/export/canonical_encoder.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::exporting {
namespace {

[[nodiscard]] std::string number(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

[[nodiscard]] std::string rgb_hex(const std::array<std::uint8_t, 3U>& rgb) {
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0') << std::nouppercase;
    for (const std::uint8_t channel : rgb) {
        stream << std::setw(2) << static_cast<unsigned int>(channel);
    }
    return stream.str();
}

[[nodiscard]] std::string encode_bytes(const ExportRequest& request) {
    const std::string width = std::to_string(request.width);
    const std::string height = std::to_string(request.height);
    switch (request.format) {
        case ExportFormat::Svg:
            return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + width + "\" height=\"" + height + "\" viewBox=\"0 0 " + width + " " + height + "\">\n</svg>\n";
        case ExportFormat::Pdf:
            return "%PDF-1.7\n% Vektoryum canonical export\n% width=" + width + " height=" + height + "\n%%EOF\n";
        case ExportFormat::Eps:
            return "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 " + width + " " + height + "\n%%EOF\n";
        case ExportFormat::Dxf:
            return "0\nSECTION\n2\nHEADER\n9\n$EXTMAX\n10\n" + width + "\n20\n" + height + "\n0\nENDSEC\n0\nEOF\n";
    }
    return {};
}

[[nodiscard]] std::string postscript_path(const vector::SvgPath& path, double height) {
    std::ostringstream out;
    vector::DoublePoint current{};
    vector::DoublePoint first{};
    bool have_point = false;
    for (const auto& command : path.commands) {
        switch (command.type) {
            case vector::SvgCommandType::MoveTo:
                current = command.end;
                first = current;
                have_point = true;
                out << number(current.x) << ' ' << number(height - current.y) << " moveto\n";
                break;
            case vector::SvgCommandType::LineTo:
                current = command.end;
                out << number(current.x) << ' ' << number(height - current.y) << " lineto\n";
                break;
            case vector::SvgCommandType::CubicTo:
                current = command.end;
                out << number(command.control1.x) << ' ' << number(height - command.control1.y) << ' '
                    << number(command.control2.x) << ' ' << number(height - command.control2.y) << ' '
                    << number(command.end.x) << ' ' << number(height - command.end.y) << " curveto\n";
                break;
            case vector::SvgCommandType::Close:
                if (have_point) {
                    current = first;
                }
                out << "closepath\n";
                break;
        }
    }
    return out.str();
}

[[nodiscard]] std::string pdf_path(const vector::SvgPath& path, double height) {
    std::string text = postscript_path(path, height);
    const std::pair<const char*, const char*> replacements[] = {
        {" moveto", " m"}, {" lineto", " l"}, {" curveto", " c"}, {"closepath", "h"}};
    for (const auto& replacement : replacements) {
        std::size_t pos = 0U;
        while ((pos = text.find(replacement.first, pos)) != std::string::npos) {
            text.replace(pos, std::char_traits<char>::length(replacement.first), replacement.second);
            pos += std::char_traits<char>::length(replacement.second);
        }
    }
    return text;
}

[[nodiscard]] std::string encode_svg_geometry(const ExportRequest& request, const vector::SvgScene& scene) {
    std::ostringstream out;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << request.width
        << "\" height=\"" << request.height << "\" viewBox=\"0 0 " << request.width << ' '
        << request.height << "\">\n";
    out << "  <path d=\"";
    bool first_subpath = true;
    for (const auto& path : scene.paths) {
        if (!first_subpath) {
            out << ' ';
        }
        out << vector::serialize_svg_path_data(path);
        first_subpath = false;
    }
    out << "\" fill=\"" << rgb_hex(scene.fill_rgb) << "\" fill-rule=\"evenodd\"/>\n";
    out << "</svg>\n";
    return out.str();
}

[[nodiscard]] std::string encode_eps_geometry(const ExportRequest& request, const vector::SvgScene& scene) {
    std::ostringstream out;
    out << "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 " << request.width << ' ' << request.height
        << "\n%%LanguageLevel: 2\n%%Pages: 1\n%%EndComments\nnewpath\n";
    for (const auto& path : scene.paths) {
        out << postscript_path(path, static_cast<double>(request.height));
    }
    out << "0 0 0 setrgbcolor\neofill\nshowpage\n%%EOF\n";
    return out.str();
}

[[nodiscard]] std::string encode_pdf_geometry(const ExportRequest& request, const vector::SvgScene& scene) {
    std::ostringstream content;
    content << "q\n0 0 0 rg\n";
    for (const auto& path : scene.paths) {
        content << pdf_path(path, static_cast<double>(request.height));
    }
    content << "f*\nQ\n";
    const std::string stream = content.str();

    const std::vector<std::string> objects{
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(request.width) + " " + std::to_string(request.height) + "] /Resources << >> /Contents 4 0 R >>",
        "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream"};

    std::string pdf = "%PDF-1.7\n% Vektoryum geometry export\n";
    std::vector<std::size_t> offsets;
    offsets.reserve(objects.size());
    for (std::size_t i = 0U; i < objects.size(); ++i) {
        offsets.push_back(pdf.size());
        pdf += std::to_string(i + 1U) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const std::size_t xref_offset = pdf.size();
    pdf += "xref\n0 " + std::to_string(objects.size() + 1U) + "\n0000000000 65535 f \n";
    for (const std::size_t offset : offsets) {
        std::ostringstream row;
        row << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
        pdf += row.str();
    }
    pdf += "trailer\n<< /Size " + std::to_string(objects.size() + 1U) + " /Root 1 0 R >>\nstartxref\n" +
           std::to_string(xref_offset) + "\n%%EOF\n";
    return pdf;
}

void emit_dxf_line(std::ostringstream& out, const vector::DoublePoint& from, const vector::DoublePoint& to) {
    out << "0\nLINE\n8\n0\n10\n" << number(from.x) << "\n20\n" << number(from.y)
        << "\n30\n0.000000\n11\n" << number(to.x) << "\n21\n" << number(to.y) << "\n31\n0.000000\n";
}

void emit_dxf_cubic(std::ostringstream& out, const vector::DoublePoint& from, const vector::SvgCommand& command) {
    out << "0\nSPLINE\n8\n0\n70\n8\n71\n3\n72\n8\n73\n4\n74\n0\n";
    constexpr double knots[] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    for (double knot : knots) {
        out << "40\n" << number(knot) << '\n';
    }
    const vector::DoublePoint points[] = {from, command.control1, command.control2, command.end};
    for (const auto& point : points) {
        out << "10\n" << number(point.x) << "\n20\n" << number(point.y) << "\n30\n0.000000\n";
    }
}

[[nodiscard]] std::string encode_dxf_geometry(const ExportRequest& request, const vector::SvgScene& scene) {
    std::ostringstream out;
    out << "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$EXTMIN\n10\n0\n20\n0\n"
        << "9\n$EXTMAX\n10\n" << request.width << "\n20\n" << request.height
        << "\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    for (const auto& path : scene.paths) {
        vector::DoublePoint current{};
        vector::DoublePoint first{};
        bool have_point = false;
        for (const auto& command : path.commands) {
            switch (command.type) {
                case vector::SvgCommandType::MoveTo:
                    current = command.end;
                    first = current;
                    have_point = true;
                    break;
                case vector::SvgCommandType::LineTo:
                    if (have_point) {
                        emit_dxf_line(out, current, command.end);
                    }
                    current = command.end;
                    break;
                case vector::SvgCommandType::CubicTo:
                    if (have_point) {
                        emit_dxf_cubic(out, current, command);
                    }
                    current = command.end;
                    break;
                case vector::SvgCommandType::Close:
                    if (have_point && !(current == first)) {
                        emit_dxf_line(out, current, first);
                    }
                    current = first;
                    break;
            }
        }
    }
    out << "0\nENDSEC\n0\nEOF\n";
    return out.str();
}

[[nodiscard]] std::string encode_geometry_bytes(const ExportRequest& request, const vector::SvgScene& scene) {
    switch (request.format) {
        case ExportFormat::Svg: return encode_svg_geometry(request, scene);
        case ExportFormat::Pdf: return encode_pdf_geometry(request, scene);
        case ExportFormat::Eps: return encode_eps_geometry(request, scene);
        case ExportFormat::Dxf: return encode_dxf_geometry(request, scene);
    }
    return {};
}

[[nodiscard]] CanonicalEncodeResult finalize_artifact(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& request_limits,
    const ExportExecutionLimits& execution_limits,
    std::string encoded) {
    if (encoded.empty()) {
        return {ExportArtifactError::InvalidStructure, {}};
    }

    EncodedExportArtifact artifact;
    artifact.format = request.format;
    artifact.export_id = request.export_id;
    artifact.source_output_id = source_output.output_id;
    artifact.source_output_sha256 = source_output.output_sha256;
    artifact.bytes.assign(encoded.begin(), encoded.end());
    artifact.output_sha256 = ml::sha256_hex(artifact.bytes);
    artifact.peak_intermediate_bytes = static_cast<std::uint64_t>(artifact.bytes.size());
    artifact.execution_units = 1U + static_cast<std::uint64_t>(artifact.bytes.size() / 1024U);

    const auto validation = validate_encoded_export_artifact(
        request, source_output, artifact, request_limits, execution_limits);
    if (!validation.ok()) {
        return {validation.error, {}};
    }
    return {ExportArtifactError::None, std::move(artifact)};
}

}  // namespace

CanonicalEncodeResult encode_canonical_export(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& request_limits,
    const ExportExecutionLimits& execution_limits) {
    if (!validate_export_request(request, source_output, request_limits).ok()) {
        return {ExportArtifactError::InvalidRequest, {}};
    }
    return finalize_artifact(
        request, source_output, request_limits, execution_limits, encode_bytes(request));
}

CanonicalEncodeResult encode_geometry_export(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const vector::SvgScene& scene,
    const ExportLimits& request_limits,
    const ExportExecutionLimits& execution_limits) {
    if (!validate_export_request(request, source_output, request_limits).ok()) {
        return {ExportArtifactError::InvalidRequest, {}};
    }
    if (scene.width != request.width || scene.height != request.height || scene.paths.empty()) {
        return {ExportArtifactError::InvalidStructure, {}};
    }
    for (const auto& path : scene.paths) {
        if (path.commands.empty() || path.commands.front().type != vector::SvgCommandType::MoveTo) {
            return {ExportArtifactError::InvalidStructure, {}};
        }
    }
    return finalize_artifact(
        request, source_output, request_limits, execution_limits, encode_geometry_bytes(request, scene));
}

}  // namespace vektoryum::exporting
