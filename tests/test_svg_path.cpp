#include <iostream>
#include <string_view>

#include "vektoryum/vector/svg_path.hpp"

namespace {
int failures = 0;
void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    } else {
        std::cout << "[PASS] " << name << '\n';
    }
}
}

int run_svg_path_tests() {
    using namespace vektoryum::vector;

    VectorScene rectangle{};
    rectangle.width = 16U;
    rectangle.height = 16U;
    rectangle.paths.push_back({{{2, 2}, {12, 2}, {12, 10}, {2, 10}}, true, false});

    const auto exact = fit_svg_paths(rectangle, {.corner_radius = 0.0});
    expect_true(exact.ok(), "polygon converts to SVG path");
    expect_true(exact.scene.paths[0].commands.size() == 5U,
                "exact polygon emits move lines and close");

    const auto rounded = fit_svg_paths(rectangle, {.corner_radius = 1.0});
    expect_true(rounded.ok(), "controlled corner fitting succeeds");
    bool has_cubic = false;
    for (const auto& command : rounded.scene.paths[0].commands) {
        has_cubic = has_cubic || command.type == SvgCommandType::CubicTo;
    }
    expect_true(has_cubic, "rounded path emits cubic segments");

    const auto rounded_repeat = fit_svg_paths(rectangle, {.corner_radius = 1.0});
    expect_true(serialize_svg_path_data(rounded.scene.paths[0]) ==
                    serialize_svg_path_data(rounded_repeat.scene.paths[0]),
                "curve fitting is deterministic");

    VectorScene donut = rectangle;
    donut.paths.push_back({{{5, 4}, {5, 8}, {9, 8}, {9, 4}}, true, true});
    const auto donut_svg = fit_svg_paths(donut);
    expect_true(donut_svg.ok() && donut_svg.scene.paths.size() == 2U &&
                    donut_svg.scene.paths[1].hole,
                "SVG scene preserves hole semantics");

    VectorScene open_scene = rectangle;
    open_scene.paths[0].closed = false;
    expect_true(fit_svg_paths(open_scene).error == SvgFitError::DegeneratePath,
                "open path is rejected");
    expect_true(fit_svg_paths(rectangle, {.corner_radius = -1.0}).error == SvgFitError::InvalidRadius,
                "negative radius is rejected");

    return failures;
}
