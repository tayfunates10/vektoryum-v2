#include "vektoryum/export/canonical_encoder.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::hybrid::HybridOutputManifest;
using vektoryum::vector::DoublePoint;
using vektoryum::vector::SvgCommand;
using vektoryum::vector::SvgCommandType;
using vektoryum::vector::SvgPath;
using vektoryum::vector::SvgScene;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

HybridOutputManifest source_output() {
    HybridOutputManifest output;
    output.output_id = "hybrid-output-1";
    output.output_sha256 = std::string(64U, 'a');
    return output;
}

ExportRequest request_for(ExportFormat format) {
    ExportRequest request;
    request.schema_version = "vektoryum.export.v1";
    request.export_id = "export-1";
    request.format = format;
    request.source_output_id = "hybrid-output-1";
    request.source_output_sha256 = std::string(64U, 'a');
    request.width = 640U;
    request.height = 480U;
    request.estimated_output_bytes = 65536U;
    return request;
}

SvgScene geometry_scene(double end_x = 100.0) {
    SvgPath path;
    SvgCommand move;
    move.type = SvgCommandType::MoveTo;
    move.end = {10.0, 10.0};
    path.commands.push_back(move);

    SvgCommand line;
    line.type = SvgCommandType::LineTo;
    line.end = {end_x, 10.0};
    path.commands.push_back(line);

    SvgCommand cubic;
    cubic.type = SvgCommandType::CubicTo;
    cubic.control1 = {120.0, 10.0};
    cubic.control2 = {120.0, 100.0};
    cubic.end = {100.0, 100.0};
    path.commands.push_back(cubic);

    SvgCommand line_back;
    line_back.type = SvgCommandType::LineTo;
    line_back.end = {10.0, 100.0};
    path.commands.push_back(line_back);

    SvgCommand close;
    close.type = SvgCommandType::Close;
    path.commands.push_back(close);

    SvgScene scene;
    scene.width = 640U;
    scene.height = 480U;
    scene.paths.push_back(path);
    return scene;
}

std::string artifact_text(const vektoryum::exporting::CanonicalEncodeResult& result) {
    return {result.artifact.bytes.begin(), result.artifact.bytes.end()};
}

void require_contains(const std::string& text, const std::string& token, const std::string& message) {
    if (text.find(token) == std::string::npos) {
        fail(message);
    }
}

}  // namespace

int main() {
    const HybridOutputManifest source = source_output();
    const std::array formats{ExportFormat::Svg, ExportFormat::Pdf, ExportFormat::Eps, ExportFormat::Dxf};

    for (const ExportFormat format : formats) {
        const ExportRequest request = request_for(format);
        const auto first = vektoryum::exporting::encode_canonical_export(request, source);
        const auto second = vektoryum::exporting::encode_canonical_export(request, source);
        if (!first.ok() || !second.ok()) {
            fail("canonical encoder rejected a valid request");
        }
        if (first.artifact.bytes != second.artifact.bytes ||
            first.artifact.output_sha256 != second.artifact.output_sha256) {
            fail("identical validated input did not produce identical bytes and digest");
        }
        const auto validation = vektoryum::exporting::validate_encoded_export_artifact(
            request, source, first.artifact);
        if (!validation.ok()) {
            fail("canonical encoder produced an artifact rejected by Stage 10 validation");
        }

        const auto geometry = vektoryum::exporting::encode_geometry_export(request, source, geometry_scene());
        const auto changed = vektoryum::exporting::encode_geometry_export(request, source, geometry_scene(96.0));
        if (!geometry.ok() || !changed.ok()) {
            fail("R5 geometry encoder rejected reconstructed scene geometry");
        }
        if (geometry.artifact.output_sha256 == changed.artifact.output_sha256) {
            fail("R5 export digest did not change when scene geometry changed");
        }
        const auto geometry_validation = vektoryum::exporting::validate_encoded_export_artifact(
            request, source, geometry.artifact);
        if (!geometry_validation.ok()) {
            fail("R5 geometry encoder produced an artifact rejected by the export contract");
        }

        const std::string text = artifact_text(geometry);
        switch (format) {
            case ExportFormat::Svg:
                require_contains(text, "<path d=\"M 10.000000 10.000000", "SVG contains no reconstructed path geometry");
                require_contains(text, " C ", "SVG lost cubic Bezier geometry");
                require_contains(text, "fill-rule=\"evenodd\"", "SVG lost topology fill rule");
                break;
            case ExportFormat::Pdf:
                require_contains(text, "xref\n", "PDF has no xref table");
                require_contains(text, "trailer\n", "PDF has no trailer dictionary");
                require_contains(text, " c\n", "PDF content stream lost cubic Bezier geometry");
                require_contains(text, "f*\n", "PDF content stream lost even-odd fill semantics");
                break;
            case ExportFormat::Eps:
                require_contains(text, "curveto\n", "EPS lost cubic Bezier geometry");
                require_contains(text, "eofill\n", "EPS lost even-odd fill semantics");
                break;
            case ExportFormat::Dxf:
                require_contains(text, "2\nENTITIES\n", "DXF has no ENTITIES section");
                require_contains(text, "0\nLINE\n", "DXF lost linear geometry");
                require_contains(text, "0\nSPLINE\n", "DXF lost cubic geometry");
                break;
        }
    }

    ExportRequest invalid = request_for(ExportFormat::Svg);
    invalid.source_output_sha256 = std::string(64U, 'b');
    if (vektoryum::exporting::encode_canonical_export(invalid, source).ok()) {
        fail("canonical encoder accepted substituted source provenance");
    }

    ExportRequest too_small = request_for(ExportFormat::Svg);
    too_small.estimated_output_bytes = 1U;
    if (vektoryum::exporting::encode_canonical_export(too_small, source).ok()) {
        fail("canonical encoder exceeded the caller output-byte budget");
    }

    SvgScene empty_scene;
    empty_scene.width = 640U;
    empty_scene.height = 480U;
    if (vektoryum::exporting::encode_geometry_export(request_for(ExportFormat::Svg), source, empty_scene).ok()) {
        fail("R5 geometry encoder accepted an empty scene");
    }

    return EXIT_SUCCESS;
}
