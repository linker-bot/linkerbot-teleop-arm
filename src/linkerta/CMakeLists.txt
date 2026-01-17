cmake_minimum_required(VERSION 3.15)
project(linkerta
    VERSION 0.0.1
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
option(BUILD_TESTING "Build the testing tree" OFF)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Architecture: ${LIB_SUBDIR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ROS2 support: ${USE_ROS2}")

#-----------------------------------------------------------------------------
# 源文件配置
#-----------------------------------------------------------------------------
set(CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CanBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LinkerArm.cpp
)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#-----------------------------------------------------------------------------
# 可执行文件：linkerta (非 ROS2 版本)
#-----------------------------------------------------------------------------
add_executable(linkerta
    ${CORE_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(linkerta PRIVATE Threads::Threads)

# 设置目标属性
set_target_properties(linkerta PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

#-----------------------------------------------------------------------------
# ROS2 支持
#-----------------------------------------------------------------------------
if(USE_ROS2)
    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(std_msgs REQUIRED)
    find_package(sensor_msgs REQUIRED)
    
    add_executable(linkerta_node
        ${CORE_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main_ros2.cpp
    )
    
    target_link_libraries(linkerta_node
        Threads::Threads
        rclcpp::rclcpp
    )
    
    target_include_directories(linkerta_node PUBLIC
        ${rclcpp_INCLUDE_DIRS}
        ${std_msgs_INCLUDE_DIRS}
    )
    
    # 设置目标属性
    set_target_properties(linkerta_node PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    ament_target_dependencies(linkerta_node rclcpp std_msgs sensor_msgs)
    
    ament_package()
endif()

#-----------------------------------------------------------------------------
# 安装配置
#-----------------------------------------------------------------------------
# 安装非 ROS2 版本的可执行文件
install(TARGETS linkerta
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

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

# 安装头文件（如果需要作为库使用）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include)
    install(DIRECTORY include/
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING PATTERN "*.h"
        COMPONENT Development
    )
endif()

#-----------------------------------------------------------------------------
# 测试支持
#-----------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
    message(STATUS "Testing enabled")
endif()
