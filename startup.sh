#!/bin/bash

# Linkerhand-Tele-Arm 启动脚本
# 启动 ROS2 节点

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

# 检查安装目录
check_install() {
    if [ ! -d "install" ]; then
        log_error "未找到 install 目录，请先运行 ./build.sh 构建项目"
        exit 1
    fi
    
    if [ ! -f "install/setup.bash" ]; then
        log_error "未找到 install/setup.bash，构建可能不完整"
        exit 1
    fi
}

# 检查 CAN 设备（可选）
check_can_device() {
    if ! ip link show can0 &> /dev/null; then
        log_warning "CAN 设备 can0 未找到或未启动"
        log_info "请运行以下命令启动 CAN 设备:"
        echo "  sudo ip link set can0 up type can bitrate 1000000"
        log_warning "继续启动节点，但可能无法正常工作..."
    else
        log_info "CAN 设备 can0 已就绪"
    fi
}

# 主函数
main() {
    log_info "启动 Linkerhand-Tele-Arm ROS2 节点..."
    
    # 检查必要的命令
    log_info "检查依赖..."
    check_command ros2
    
    # 检查 ROS2 环境
    check_ros2
    
    # 检查安装目录
    check_install
    
    # Source 安装目录
    log_info "设置工作空间环境..."
    source install/setup.bash
    
    # 检查 CAN 设备
    check_can_device
    
    # 检查节点是否存在
    if ! ros2 pkg list | grep -q linkerta; then
        log_error "linkerta 包未找到，请先运行 ./build.sh 构建项目"
        exit 1
    fi
    
    # 启动节点
    log_success "启动 linkerta_node..."
    log_info "使用 launch 文件: run.launch.py"
    log_info "按 Ctrl+C 停止节点"
    log_info ""
    
    if ros2 launch linkerta run.launch.py; then
        log_success "节点已正常退出"
    else
        log_error "节点启动失败或异常退出"
        exit 1
    fi
}

# 运行主函数
main "$@"
