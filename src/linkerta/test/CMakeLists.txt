# 测试CMake配置
cmake_minimum_required(VERSION 3.18)

# 查找Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    if(NOT LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS AND NOT FETCHCONTENT_SOURCE_DIR_GOOGLETEST)
        message(FATAL_ERROR "GoogleTest is missing; install it or set FETCHCONTENT_SOURCE_DIR_GOOGLETEST (downloads disabled)")
    endif()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 测试源文件
set(TEST_SOURCES test_protocol.cpp test_runtime.cpp test_node_config.cpp test_canbus_io.cpp test_session.cpp)
add_executable(run_tests ${TEST_SOURCES})

# 查找Threads库
find_package(Threads REQUIRED)

# 链接库
target_link_libraries(run_tests
    PRIVATE
    linkerta_core
    GTest::gtest_main
    Threads::Threads
)

# 如果是Linux系统，添加必要的链接库
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(run_tests PRIVATE rt)
endif()

# 设置C++标准
set_target_properties(run_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(run_tests DISCOVERY_MODE PRE_TEST)

# 代码覆盖率支持（如果可用）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(LCOV_PATH AND GENHTML_PATH)
        message(STATUS "Code coverage tools found")
        
        # 添加覆盖率编译选项
        target_compile_options(linkerta_core PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(linkerta_core PUBLIC --coverage)
        target_compile_options(run_tests PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        
        target_link_options(run_tests PRIVATE
            --coverage
        )
        
        # 添加覆盖率目标
        add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --no-tests=error
            COMMAND ${LCOV_PATH} --capture --directory ${CMAKE_BINARY_DIR} --output-file ${CMAKE_BINARY_DIR}/coverage.info
            COMMAND ${LCOV_PATH} --remove ${CMAKE_BINARY_DIR}/coverage.info "/usr/*" "*/test/*" "*/googletest/*" --output-file ${CMAKE_BINARY_DIR}/coverage.info
            COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage_html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
            DEPENDS run_tests
            VERBATIM
        )
    endif()
endif()

if(USE_ROS2)
    add_executable(run_ros2_tests test_ros2_node.cpp)
    target_link_libraries(run_ros2_tests PRIVATE linkerta_ros GTest::gtest_main)
    ament_target_dependencies(run_ros2_tests rclcpp sensor_msgs diagnostic_msgs std_srvs)
    gtest_discover_tests(run_ros2_tests DISCOVERY_MODE PRE_TEST)
    add_executable(ros2_test_fixture ros2_test_fixture.cpp)
    target_link_libraries(ros2_test_fixture PRIVATE linkerta_ros)
    ament_target_dependencies(ros2_test_fixture rclcpp std_msgs std_srvs)
    find_package(launch_testing_ament_cmake REQUIRED)
    add_launch_test(test_ros2_launch.py
        ARGS "fixture_path:=$<TARGET_FILE:ros2_test_fixture>"
        TIMEOUT 60)

endif()
