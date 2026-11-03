cmake_minimum_required(VERSION 3.18)
project(linkerta
    VERSION 2.0.0
    LANGUAGES CXX
    DESCRIPTION "Linker Tele-Arm driver for robotic arm control"
)

# 检测系统架构
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(LIB_SUBDIR "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(LIB_SUBDIR "aarch64")
else()
    message(WARNING "Unknown architecture, defaulting to x86_64")
    set(LIB_SUBDIR "x86_64")
endif()

# C++ 标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译警告选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#-----------------------------------------------------------------------------
# 构建选项
#-----------------------------------------------------------------------------
option(USE_ROS2 "Build with ROS2 support" OFF)
include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS "Download missing dependencies" ON)
find_package(linkerbot-comm 0.3 CONFIG QUIET)
if(NOT TARGET linkerbot::comm)
    if(NOT LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS AND NOT FETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT)
        message(FATAL_ERROR "linkerbot-comm missing; install it or set FETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT")
    endif()
    include(FetchContent)
    set(LINKERBOT_COMM_BUILD_TESTS OFF)
    set(LINKERBOT_COMM_BUILD_EXAMPLES OFF)
    set(LINKERBOT_COMM_BUILD_BENCHMARKS OFF)
    FetchContent_Declare(linkerbot_transport
        GIT_REPOSITORY https://github.com/soulteary/linkerbot-transport-cpp.git
        GIT_TAG e514b832e09ecd60a56d2c67e39c54285793fff8
    )
    FetchContent_MakeAvailable(linkerbot_transport)
endif()


message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Architecture: ${LIB_SUBDIR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ROS2 support: ${USE_ROS2}")

#-----------------------------------------------------------------------------
# 源文件配置
#-----------------------------------------------------------------------------
set(CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LinkerArm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CliOptions.cpp
)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#-----------------------------------------------------------------------------
# 可执行文件：linkerta (非 ROS2 版本)
#-----------------------------------------------------------------------------
find_package(Threads REQUIRED)
add_library(linkerta_core ${CORE_SRCS} src/ArmProtocol.cpp)
add_library(linkerta::core ALIAS linkerta_core)
set_target_properties(linkerta_core PROPERTIES EXPORT_NAME core)
target_compile_features(linkerta_core PUBLIC cxx_std_17)
target_include_directories(linkerta_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(linkerta_core PUBLIC Threads::Threads linkerbot::comm)
if(APPLE)
    set(LINKERTA_LIBRARY_RPATH "@loader_path")
    set(LINKERTA_BINARY_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    set(LINKERTA_ROS_RPATH "@loader_path/../../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
    set(LINKERTA_LIBRARY_RPATH "$ORIGIN")
    set(LINKERTA_BINARY_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    set(LINKERTA_ROS_RPATH "$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
endif()
set_target_properties(linkerta_core PROPERTIES INSTALL_RPATH "${LINKERTA_LIBRARY_RPATH}")
add_executable(linkerta src/main.cpp)
target_link_libraries(linkerta PRIVATE linkerta_core)
set_target_properties(linkerta PROPERTIES INSTALL_RPATH "${LINKERTA_BINARY_RPATH}")

#-----------------------------------------------------------------------------
# ROS2 支持
#-----------------------------------------------------------------------------
if(USE_ROS2)
    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(std_msgs REQUIRED)
    find_package(sensor_msgs REQUIRED)
    find_package(diagnostic_msgs REQUIRED)
    find_package(std_srvs REQUIRED)
    
    add_library(linkerta_ros src/TeleArmNode.cpp)
    target_link_libraries(linkerta_ros PUBLIC linkerta_core)
    set_target_properties(linkerta_ros PROPERTIES INSTALL_RPATH "${LINKERTA_LIBRARY_RPATH}")
    ament_target_dependencies(linkerta_ros PUBLIC rclcpp std_msgs sensor_msgs diagnostic_msgs std_srvs)

    add_executable(linkerta_node src/main_ros2.cpp)
    target_link_libraries(linkerta_node PRIVATE linkerta_ros)
    set_target_properties(linkerta_node PROPERTIES INSTALL_RPATH "${LINKERTA_ROS_RPATH}")
    ament_target_dependencies(linkerta_node PUBLIC rclcpp)

endif()

#-----------------------------------------------------------------------------
# 安装配置
#-----------------------------------------------------------------------------
# 安装非 ROS2 版本的可执行文件
if(TARGET linkerta)
install(TARGETS linkerta
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)
endif()

# ROS2 版本安装
if(USE_ROS2)
    install(TARGETS linkerta_node
        RUNTIME DESTINATION lib/${PROJECT_NAME}
        COMPONENT Runtime
    )
    
    install(DIRECTORY launch
        DESTINATION share/${PROJECT_NAME}
        COMPONENT Runtime
    )
    
    install(DIRECTORY config
        DESTINATION share/${PROJECT_NAME}
        COMPONENT Runtime
    )
endif()

# Keep project and bundled dependency notices with installed binaries.
set(LINKERTA_LICENSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../")
install(FILES "${LINKERTA_LICENSE_DIR}/LICENSE" "${LINKERTA_LICENSE_DIR}/THIRD_PARTY_NOTICES.md" "${LINKERTA_LICENSE_DIR}/THIRD_PARTY_NOTICES.zh-CN.md"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/linkerta COMPONENT Runtime)
install(DIRECTORY "${LINKERTA_LICENSE_DIR}/LICENSES/"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/linkerta/LICENSES COMPONENT Runtime)

# Install the public core API and a relocatable imported target.
install(TARGETS linkerta_core EXPORT linkertaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES src/LinkerArm.h src/ArmProtocol.h src/SessionFactory.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linkerta)
if(USE_ROS2)
    # Let ament own linkertaConfig.cmake; its extras load the same core API.
    set(LINKERTA_CONFIG_DESTINATION share/${PROJECT_NAME}/cmake)
    install(TARGETS linkerta_ros
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    set(LINKERTA_CONFIG_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
    configure_package_config_file(cmake/linkertaConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/linkertaConfig.cmake
        INSTALL_DESTINATION ${LINKERTA_CONFIG_DESTINATION})
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/linkertaConfigVersion.cmake
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/linkertaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/linkertaConfigVersion.cmake
        DESTINATION ${LINKERTA_CONFIG_DESTINATION})
endif()
install(EXPORT linkertaTargets NAMESPACE linkerta::
    DESTINATION ${LINKERTA_CONFIG_DESTINATION})
install(DIRECTORY test/consumer/ DESTINATION ${CMAKE_INSTALL_DATADIR}/linkerta/examples/consumer)

# Optional measurement harness; synthetic output is explicitly marked as such.
option(LINKERTA_BUILD_BENCHMARKS "Build the explicit opt-in measurement harness" OFF)
if(LINKERTA_BUILD_BENCHMARKS)
    add_library(linkerta_benchmark_options STATIC tools/BenchmarkOptions.cpp)
    target_include_directories(linkerta_benchmark_options PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    add_executable(linkerta_benchmark tools/benchmark.cpp)
    target_link_libraries(linkerta_benchmark PRIVATE linkerta_core linkerta_benchmark_options)
    set_target_properties(linkerta_benchmark PROPERTIES INSTALL_RPATH "${LINKERTA_BINARY_RPATH}")
    find_package(Git QUIET)
    set(LINKERTA_BUILD_REVISION "unknown")
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse HEAD
            OUTPUT_VARIABLE LINKERTA_BUILD_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
    if(NOT LINKERTA_BUILD_REVISION MATCHES "^[0-9a-f]+$")
        set(LINKERTA_BUILD_REVISION "unknown")
    endif()
    target_compile_definitions(linkerta_benchmark PRIVATE LINKERTA_BUILD_REVISION="${LINKERTA_BUILD_REVISION}")
    install(TARGETS linkerta_benchmark RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

#-----------------------------------------------------------------------------
# 测试支持
#-----------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
    configure_file(test/install_consumer.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/install_consumer.cmake @ONLY)
    add_test(NAME installed-core-consumer
        COMMAND ${CMAKE_COMMAND}
            -DCONFIG=$<CONFIG>
            -P ${CMAKE_CURRENT_BINARY_DIR}/install_consumer.cmake)
    set_tests_properties(installed-core-consumer PROPERTIES TIMEOUT 120)
    message(STATUS "Testing enabled")
endif()

if(USE_ROS2)
    ament_package(CONFIG_EXTRAS cmake/linkerta-core-extras.cmake.in)
endif()
