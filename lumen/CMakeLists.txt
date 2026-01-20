cmake_minimum_required(VERSION 3.16)

# vcpkg toolchain (must be before project())
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(lumen
    VERSION 1.0.0
    DESCRIPTION "Lumen - Quantum-Enhanced Edge Reasoning Engine for Portfolio Optimization"
    HOMEPAGE_URL "https://github.com/oaqlabs/lumen"
    LANGUAGES CXX)

# ============================================================================
# C++ Standard Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Type Configuration
# ============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# ============================================================================
# Build Options
# ============================================================================
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_CLI "Build command-line interface" ON)
option(BUILD_SERVER "Build REST API server" ON)
option(BUILD_GUI "Build Qt desktop GUI" OFF)
option(ENABLE_QUANTUM "Enable quantum solver support (D-Wave, IBM)" ON)
option(ENABLE_PYFLARE "Enable PyFlare telemetry integration" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

# ============================================================================
# Code Coverage (optional)
# ============================================================================
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# ============================================================================
# Find Dependencies
# ============================================================================

# Required dependencies
find_package(Eigen3 3.3 REQUIRED)
find_package(nlohmann_json 3.9 REQUIRED)
find_package(SQLite3 REQUIRED)

# HiGHS optimization solver
find_package(highs CONFIG QUIET)
if(NOT highs_FOUND)
    find_package(HIGHS QUIET)
endif()
if(NOT highs_FOUND AND NOT HIGHS_FOUND)
    message(WARNING "HiGHS not found. Solver functionality will be limited.")
    set(HIGHS_AVAILABLE OFF)
else()
    set(HIGHS_AVAILABLE ON)
    message(STATUS "HiGHS found")
endif()

# SymEngine for symbolic mathematics (optional for Phase 1)
find_package(SymEngine QUIET)
if(SymEngine_FOUND)
    set(SYMENGINE_AVAILABLE ON)
    message(STATUS "SymEngine found")
else()
    set(SYMENGINE_AVAILABLE OFF)
    message(STATUS "SymEngine not found - symbolic features disabled")
endif()

# yaml-cpp for configuration
find_package(yaml-cpp QUIET)
if(yaml-cpp_FOUND)
    set(YAML_AVAILABLE ON)
else()
    set(YAML_AVAILABLE OFF)
    message(STATUS "yaml-cpp not found - will use JSON for config")
endif()

# CLI11 for command-line parsing
find_package(CLI11 QUIET)
if(NOT CLI11_FOUND)
    message(STATUS "CLI11 not found - fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.3.2
    )
    FetchContent_MakeAvailable(cli11)
endif()

# cpp-httplib for HTTP client/server (header-only)
find_path(HTTPLIB_INCLUDE_DIR NAMES httplib.h)
if(NOT HTTPLIB_INCLUDE_DIR)
    message(STATUS "cpp-httplib not found - fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.3
    )
    FetchContent_MakeAvailable(httplib)
    set(HTTPLIB_INCLUDE_DIR ${httplib_SOURCE_DIR})
endif()

# Google Test for testing
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "Google Test not found - fetching from GitHub")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # Prevent overriding parent project's compiler/linker settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    enable_testing()
    include(GoogleTest)
endif()

# Google Benchmark for performance testing
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found - fetching from GitHub")
        include(FetchContent)
        FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.8.3
        )
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(benchmark)
    endif()
endif()

# Qt6 for GUI (optional)
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets Charts QUIET)
    if(Qt6_FOUND)
        set(QT_AVAILABLE ON)
        message(STATUS "Qt6 found")
    else()
        set(QT_AVAILABLE OFF)
        message(WARNING "Qt6 not found - GUI will not be built")
        set(BUILD_GUI OFF)
    endif()
endif()

# ============================================================================
# Configure Header
# ============================================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/lumen/config.hpp
    @ONLY
)

# ============================================================================
# Core Library
# ============================================================================
add_library(lumen-core STATIC
    # Core module
    src/core/portfolio.cpp
    src/core/constraint.cpp
    src/core/solver_dispatcher.cpp

    # Solvers module
    src/solvers/highs_wrapper.cpp
    src/solvers/quantum_client.cpp
    src/solvers/quantum_mock.cpp
    src/solvers/hybrid_orchestrator.cpp

    # Data module
    src/data/market_data.cpp
    src/data/tax_lot.cpp
    src/data/persistence.cpp

    # Explain module
    src/explain/provenance.cpp
    src/explain/explainer.cpp

    # Utils module
    src/utils/logging.cpp
    src/utils/config.cpp
    src/utils/pyflare.cpp
)

target_include_directories(lumen-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(lumen-core
    PUBLIC
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
)

if(HIGHS_AVAILABLE)
    target_link_libraries(lumen-core PUBLIC highs::highs)
    target_compile_definitions(lumen-core PUBLIC LUMEN_HAS_HIGHS)
endif()

if(SYMENGINE_AVAILABLE)
    target_link_libraries(lumen-core PUBLIC symengine)
    target_compile_definitions(lumen-core PUBLIC LUMEN_HAS_SYMENGINE)
endif()

if(YAML_AVAILABLE)
    target_link_libraries(lumen-core PUBLIC yaml-cpp)
    target_compile_definitions(lumen-core PUBLIC LUMEN_HAS_YAML)
endif()

# ============================================================================
# CLI Application
# ============================================================================
if(BUILD_CLI)
    add_executable(lumen-cli
        src/cli/main.cpp
    )

    target_link_libraries(lumen-cli
        PRIVATE
            lumen-core
            CLI11::CLI11
    )

    set_target_properties(lumen-cli PROPERTIES
        OUTPUT_NAME "lumen"
    )
endif()

# ============================================================================
# REST API Server
# ============================================================================
if(BUILD_SERVER)
    add_executable(lumen-server
        src/server/main.cpp
    )

    target_include_directories(lumen-server
        PRIVATE
            ${HTTPLIB_INCLUDE_DIR}
    )

    target_link_libraries(lumen-server
        PRIVATE
            lumen-core
            CLI11::CLI11
    )

    # Link pthread on Linux for httplib
    if(UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)
        target_link_libraries(lumen-server PRIVATE Threads::Threads)
    endif()
endif()

# ============================================================================
# Qt GUI Application
# ============================================================================
if(BUILD_GUI AND QT_AVAILABLE)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(GUI_SOURCES
        # Main application
        apps/gui/main.cpp
        apps/gui/mainwindow.cpp
        apps/gui/applicationstate.cpp
        apps/gui/settingsdialog.cpp
        apps/gui/aboutdialog.cpp

        # Dialogs
        apps/gui/dialogs/positiondialog.cpp
        apps/gui/dialogs/importdialog.cpp

        # Models
        apps/gui/models/portfoliomodel.cpp

        # Views
        apps/gui/views/portfoliotableview.cpp
        apps/gui/views/allocationchartview.cpp
        apps/gui/views/targeteditor.cpp

        # Panels
        apps/gui/panels/constraintpanel.cpp
        apps/gui/panels/resultspanel.cpp
        apps/gui/panels/comparisonpanel.cpp

        # Workers
        apps/gui/workers/optimizationworker.cpp

        # Widgets
        apps/gui/widgets/progresswidget.cpp
    )

    set(GUI_HEADERS
        # Main application
        apps/gui/mainwindow.hpp
        apps/gui/applicationstate.hpp
        apps/gui/settingsdialog.hpp
        apps/gui/aboutdialog.hpp

        # Dialogs
        apps/gui/dialogs/positiondialog.hpp
        apps/gui/dialogs/importdialog.hpp

        # Models
        apps/gui/models/portfoliomodel.hpp

        # Views
        apps/gui/views/portfoliotableview.hpp
        apps/gui/views/allocationchartview.hpp
        apps/gui/views/targeteditor.hpp

        # Panels
        apps/gui/panels/constraintpanel.hpp
        apps/gui/panels/resultspanel.hpp
        apps/gui/panels/comparisonpanel.hpp

        # Workers
        apps/gui/workers/optimizationworker.hpp

        # Widgets
        apps/gui/widgets/progresswidget.hpp
    )

    set(GUI_RESOURCES
        apps/gui/resources/lumen.qrc
    )

    # Windows-specific: add application icon resource
    if(WIN32)
        set(GUI_RESOURCES ${GUI_RESOURCES}
            apps/gui/resources/lumen.rc
        )
    endif()

    add_executable(lumen-gui WIN32
        ${GUI_SOURCES}
        ${GUI_HEADERS}
        ${GUI_RESOURCES}
    )

    target_include_directories(lumen-gui
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/apps/gui
    )

    target_link_libraries(lumen-gui
        PRIVATE
            lumen-core
            Qt6::Widgets
            Qt6::Charts
    )

    # Platform-specific settings
    if(WIN32)
        # Windows 11+ manifest for DPI awareness
        set_target_properties(lumen-gui PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()

    # Installation for GUI
    install(TARGETS lumen-gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
    )

    # Deploy Qt dependencies on Windows
    if(WIN32)
        # Qt deployment tool
        get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_qmake_location}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET lumen-gui POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E
                    env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                        --no-compiler-runtime
                        --no-translations
                        "$<TARGET_FILE:lumen-gui>"
                COMMENT "Deploying Qt dependencies..."
            )
        endif()
    endif()
endif()

# ============================================================================
# CPack Packaging Configuration
# ============================================================================
if(BUILD_GUI AND QT_AVAILABLE)
    include(cmake/packaging.cmake OPTIONAL)
endif()

# ============================================================================
# Tests
# ============================================================================
if(BUILD_TESTS)
    add_executable(lumen-tests
        # Core tests
        tests/core/test_portfolio.cpp
        tests/core/test_constraint.cpp
        tests/core/test_solver_dispatcher.cpp

        # Solvers tests
        tests/solvers/test_qubo_formulator.cpp
        tests/solvers/test_dwave_client.cpp
        tests/solvers/test_hybrid_orchestrator.cpp
        tests/solvers/test_ibm_quantum.cpp

        # Data tests
        tests/data/test_tax_lot.cpp
        tests/data/test_broker_import.cpp

        # Utils tests
        tests/utils/test_logging.cpp
        tests/utils/test_config.cpp

        # Integration tests
        tests/integration/test_tax_optimization.cpp
    )

    target_link_libraries(lumen-tests
        PRIVATE
            lumen-core
            GTest::gtest
            GTest::gtest_main
    )

    gtest_discover_tests(lumen-tests)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(BUILD_BENCHMARKS)
    add_executable(lumen-benchmarks
        test/benchmarks/solver_benchmark.cpp
        test/benchmarks/benchmark_main.cpp
    )

    target_link_libraries(lumen-benchmarks
        PRIVATE
            lumen-core
            benchmark::benchmark
    )
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS lumen-core
    EXPORT lumen-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_CLI)
    install(TARGETS lumen-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(BUILD_SERVER)
    install(TARGETS lumen-server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ============================================================================
# Package Configuration
# ============================================================================
install(EXPORT lumen-targets
    FILE lumen-targets.cmake
    NAMESPACE lumen::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumen
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lumen-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/lumen-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumen
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/lumen-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lumen-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lumen-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumen
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "Lumen Configuration Summary")
message(STATUS "===========================")
message(STATUS "Version:          ${PROJECT_VERSION}")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  HiGHS solver:   ${HIGHS_AVAILABLE}")
message(STATUS "  SymEngine:      ${SYMENGINE_AVAILABLE}")
message(STATUS "  YAML config:    ${YAML_AVAILABLE}")
message(STATUS "  Quantum:        ${ENABLE_QUANTUM}")
message(STATUS "  PyFlare:        ${ENABLE_PYFLARE}")
message(STATUS "")
message(STATUS "Build targets:")
message(STATUS "  CLI:            ${BUILD_CLI}")
message(STATUS "  Server:         ${BUILD_SERVER}")
message(STATUS "  GUI:            ${BUILD_GUI}")
message(STATUS "  Tests:          ${BUILD_TESTS}")
message(STATUS "  Benchmarks:     ${BUILD_BENCHMARKS}")
message(STATUS "")
