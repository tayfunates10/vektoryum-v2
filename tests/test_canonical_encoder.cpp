#include "vektoryum/export/canonical_encoder.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

#include "vektoryum/ml/artifact_digest.hpp"

namespace {

using vektoryum::exporting::ExportArtifactError;
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

SvgPath rectangle_path(double left, double top, double right, double bottom) {
    SvgPath path;
    SvgCommand move;
    move.type = SvgCommandType::MoveTo;
    move.end = {left, top};
    path.commands.push_back(move);

    for (const DoublePoint point : std::array<DoublePoint, 3U>{
             DoublePoint{right, top}, DoublePoint{right, bottom}, DoublePoint{left, bottom}}) {
        SvgCommand line;
        line.type = SvgCommandType::LineTo;
        line.end = point;
        path.commands.push_back(line);
    }

    SvgCommand close;
    close.type = SvgCommandType::Close;
    path.commands.push_back(close);
    return path;
}

SvgScene compound_hole_scene() {
    SvgScene scene;
    scene.width = 640U;
    scene.height = 480U;
    scene.paths.push_back(rectangle_path(10.0, 10.0, 200.0, 200.0));
    scene.paths.push_back(rectangle_path(60.0, 60.0, 150.0, 150.0));
    scene.paths.push_back(rectangle_path(90.0, 90.0, 120.0, 120.0));
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

vektoryum::exporting::EncodedExportArtifact malformed_copy(
    const vektoryum::exporting::EncodedExportArtifact& artifact,
    const std::string& token,
    const std::string& replacement) {
    std::string text{artifact.bytes.begin(), artifact.bytes.end()};
    const std::size_t position = text.find(token);
    if (position == std::string::npos) {
        fail("malformation target token was not present");
    }
    text.replace(position, token.size(), replacement);
    auto malformed = artifact;
    malformed.bytes.assign(text.begin(), text.end());
    malformed.output_sha256 = vektoryum::ml::sha256_hex(malformed.bytes);
    return malformed;
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
        vektoryum::exporting::EncodedExportArtifact malformed;
        switch (format) {
            case ExportFormat::Svg: {
                require_contains(text, "<path d=\"M 10.000000 10.000000", "SVG contains no reconstructed path geometry");
                require_contains(text, " C ", "SVG lost cubic Bezier geometry");
                require_contains(text, "fill-rule=\"evenodd\"", "SVG lost topology fill rule");

                const auto compound = vektoryum::exporting::encode_geometry_export(
                    request, source, compound_hole_scene());
                if (!compound.ok()) {
                    fail("U3 compound hole scene was rejected by SVG encoder");
                }
                const std::string compound_text = artifact_text(compound);
                const std::size_t first_path = compound_text.find("<path d=\"");
                if (first_path == std::string::npos ||
                    compound_text.find("<path d=\"", first_path + 1U) != std::string::npos) {
                    fail("U3 SVG serialized compound contours as separate path elements");
                }
                require_contains(
                    compound_text,
                    "Z M 60.000000 60.000000",
                    "U3 SVG compound path did not preserve the nested hole subpath");
                require_contains(
                    compound_text,
                    "Z M 90.000000 90.000000",
                    "U3 SVG compound path did not preserve the nested island subpath");
                require_contains(
                    compound_text,
                    "fill-rule=\"evenodd\"",
                    "U3 SVG compound path lost even-odd hole semantics");

                malformed = malformed_copy(geometry.artifact, "fill-rule=\"evenodd\"", "fill-rule=\"nonzero\"");
                break;
            }
            case ExportFormat::Pdf:
                require_contains(text, "xref\n", "PDF has no xref table");
                require_contains(text, "trailer\n", "PDF has no trailer dictionary");
                require_contains(text, " c\n", "PDF content stream lost cubic Bezier geometry");
                require_contains(text, "f*\n", "PDF content stream lost even-odd fill semantics");
                malformed = malformed_copy(geometry.artifact, "xref\n", "xrex\n");
                break;
            case ExportFormat::Eps:
                require_contains(text, "curveto\n", "EPS lost cubic Bezier geometry");
                require_contains(text, "eofill\n", "EPS lost even-odd fill semantics");
                malformed = malformed_copy(geometry.artifact, "%%EndComments\n", "%%BadComments\n");
                break;
            case ExportFormat::Dxf:
                require_contains(text, "2\nENTITIES\n", "DXF has no ENTITIES section");
                require_contains(text, "0\nLINE\n", "DXF lost linear geometry");
                require_contains(text, "0\nSPLINE\n", "DXF lost cubic geometry");
                malformed = malformed_copy(geometry.artifact, "2\nENTITIES\n", "2\nENTITIEZ\n");
                break;
        }
        const auto malformed_validation = vektoryum::exporting::validate_encoded_export_artifact(
            request, source, malformed);
        if (malformed_validation.error != ExportArtifactError::InvalidStructure) {
            fail("R5 format-aware validator accepted a malformed geometry artifact");
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
