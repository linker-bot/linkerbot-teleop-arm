# C++ Report Card

English · [简体中文](zh/QUALITY.md) · [Documentation](README.md)

The dedicated CI workflow builds a compilation database and runs clang-format,
clang-tidy and Cppcheck through the pinned C++ Report Card action. Reports are
uploaded as CI artifacts; the workflow does not commit generated grades or badges.
Use a completed run for a specific commit when quoting a result.

The analyzer build uses ROS2 Jazzy on Ubuntu 24.04 to obtain the analysis toolchain.
The runtime test job continues to use Humble; this analysis build does not extend
the documented hardware or ROS deployment support.

Analysis covers this project's core, CLI, ROS2 adapter, measurement tool and tests.
Downloaded dependencies and generated build files are not this project's analysis
scope. Compiler include paths remain available when analyzing project sources.
A test-framework model, if used, must have regression checks showing that it does
not hide real defects.

The gate requires a complete full-mode report and zero findings from all configured
tools. Tool failures, incomplete analysis and diagnostics fail the job. Do not lower
the threshold or remove files to obtain a grade; fix or explain a diagnostic first.

Static analysis complements the runtime suite in [TESTING.md](TESTING.md). It does
not measure test coverage or replace [hardware validation](VALIDATION.md).
See `.cppreportcard.json` and `.github/workflows/cppreportcard.yml` for exact tool
settings and build inputs.
