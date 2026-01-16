#!/bin/bash

# Linkerhand-Tele-Arm 构建脚本
# 支持 ROS2 和非 ROS2 模式构建

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查命令是否存在
check_command() {
    if ! command -v $1 &> /dev/null; then
        log_error "$1 未安装，请先安装 $1"
        exit 1
    fi
}

# 检查 ROS2 环境
check_ros2() {
    if [ -z "$ROS_DISTRO" ]; then
        log_warning "ROS2 环境未设置，尝试自动设置..."
        if [ -f "/opt/ros/humble/setup.bash" ]; then
            source /opt/ros/humble/setup.bash
            log_info "已自动设置 ROS2 Humble 环境"
        elif [ -f "/opt/ros/foxy/setup.bash" ]; then
            source /opt/ros/foxy/setup.bash
            log_info "已自动设置 ROS2 Foxy 环境"
        else
            log_error "未找到 ROS2 环境，请先 source ROS2 setup.bash"
            exit 1
        fi
    else
        log_info "ROS2 环境: $ROS_DISTRO"
    fi
}

# 主函数
main() {
    log_info "开始构建 Linkerhand-Tele-Arm..."
    
    # 检查必要的命令
    log_info "检查构建依赖..."
    check_command cmake
    check_command colcon
    
    # 检查 ROS2 环境
    check_ros2
    
    # 检查当前目录
    if [ ! -f "src/linkerta/CMakeLists.txt" ]; then
        log_error "未找到 CMakeLists.txt，请确保在项目根目录运行此脚本"
        exit 1
    fi
    
    # 清理之前的构建（可选）
    if [ "$1" == "clean" ]; then
        log_info "清理之前的构建..."
        rm -rf build install log
    fi
    
    # 构建项目
    log_info "开始构建项目（ROS2 模式）..."
    log_info "使用 CMake 参数: -DUSE_ROS2=ON"
    
    if colcon build --cmake-args -DUSE_ROS2=ON; then
        log_success "构建完成！"
        log_info "构建产物位置:"
        echo "  - 可执行文件: install/linkerta/lib/linkerta/linkerta_node"
        echo "  - 配置文件: install/linkerta/share/linkerta/config/"
        echo "  - Launch 文件: install/linkerta/share/linkerta/launch/"
        log_info ""
        log_info "运行以下命令以使用项目:"
        echo "  source install/setup.bash"
        echo "  ros2 launch linkerta run.launch.py"
    else
        log_error "构建失败！"
        exit 1
    fi
}

# 运行主函数
main "$@"
