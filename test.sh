#!/bin/bash

# 单元测试运行脚本
# 用法: ./test.sh [选项]
# 选项:
#   -c, --coverage    生成代码覆盖率报告
#   -v, --verbose     显示详细输出
#   -h, --help        显示帮助信息

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 默认选项
COVERAGE=false
VERBOSE=false
BUILD_TYPE=Debug

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--coverage)
            COVERAGE=true
            BUILD_TYPE=Debug
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            echo "用法: $0 [选项]"
            echo "选项:"
            echo "  -c, --coverage    生成代码覆盖率报告"
            echo "  -v, --verbose     显示详细输出"
            echo "  -h, --help        显示帮助信息"
            exit 0
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            exit 1
            ;;
    esac
done

# 检查依赖
echo -e "${YELLOW}检查依赖...${NC}"
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}错误: 未找到 cmake${NC}"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${RED}错误: 未找到 make${NC}"
    exit 1
fi

# 创建构建目录
BUILD_DIR="build_test"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}清理旧的构建目录...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置CMake
echo -e "${YELLOW}配置CMake...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTING=ON"
if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

cmake ../src/linkerta $CMAKE_ARGS

# 构建
echo -e "${YELLOW}构建测试...${NC}"
if [ "$VERBOSE" = true ]; then
    make -j$(nproc)
else
    make -j$(nproc) 2>&1 | grep -E "(error|warning|Building|Linking)" || true
fi

# 运行测试
echo -e "${YELLOW}运行测试...${NC}"
if [ "$VERBOSE" = true ]; then
    ctest --output-on-failure
else
    ctest --output-on-failure --quiet
fi

# 生成覆盖率报告
if [ "$COVERAGE" = true ]; then
    echo -e "${YELLOW}生成代码覆盖率报告...${NC}"
    
    if command -v lcov &> /dev/null && command -v genhtml &> /dev/null; then
        make coverage
        if [ -d "coverage_html" ]; then
            echo -e "${GREEN}覆盖率报告已生成: $BUILD_DIR/coverage_html/index.html${NC}"
        else
            echo -e "${YELLOW}警告: 覆盖率报告生成失败${NC}"
        fi
    else
        echo -e "${YELLOW}警告: 未找到 lcov 或 genhtml，跳过覆盖率报告生成${NC}"
        echo -e "${YELLOW}安装命令: sudo apt-get install lcov${NC}"
    fi
fi

# 显示测试结果摘要
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}测试完成！${NC}"
echo -e "${GREEN}========================================${NC}"

cd ..
