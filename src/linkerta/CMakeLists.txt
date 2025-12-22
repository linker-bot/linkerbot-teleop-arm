cmake_minimum_required(VERSION 3.10)
project(linkerta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#------------------------------------------
# 开关：是否启用 ROS2 环境
#------------------------------------------
option(USE_ROS2 "Build with ROS2 support" OFF)

#==========================================
set(CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CanBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LinkerArm.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#==========================================
add_executable(linkerta
    ${CORE_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(linkerta PRIVATE Threads::Threads)

#==========================================
# ROS2
#==========================================
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
    
    ament_target_dependencies(linkerta_node rclcpp std_msgs sensor_msgs)
    
    ament_package()
endif()

#==========================================
# INSTALL
#==========================================
install(TARGETS linkerta
    RUNTIME DESTINATION bin
)
if(USE_ROS2)
    install(TARGETS linkerta_node
        RUNTIME DESTINATION lib/${PROJECT_NAME}
    )
    install (DIRECTORY launch
      DESTINATION share/${PROJECT_NAME}
    )
    install (DIRECTORY config
      DESTINATION share/${PROJECT_NAME}
    )
endif()
