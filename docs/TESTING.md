# 单元测试文档

本文档介绍如何运行和编写 LinkerArm 项目的单元测试。

## 📋 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [运行测试](#运行测试)
- [代码覆盖率](#代码覆盖率)
- [编写测试](#编写测试)
- [测试结构](#测试结构)
- [故障排查](#故障排查)

## 概述

项目使用 [Google Test](https://github.com/google/googletest) 作为测试框架，提供了全面的单元测试支持。

### 测试框架特性

- ✅ 单元测试覆盖核心功能
- ✅ Mock 对象支持（用于硬件依赖隔离）
- ✅ 代码覆盖率报告
- ✅ 持续集成支持

## 快速开始

### 前置要求

1. **CMake 3.15+**
   ```bash
   cmake --version
   ```

2. **C++17 兼容的编译器** (GCC 7.0+ 或 Clang 5.0+)
   ```bash
   g++ --version
   ```

3. **Google Test** (自动下载，无需手动安装)

4. **可选：代码覆盖率工具** (用于生成覆盖率报告)
   ```bash
   sudo apt-get install lcov
   ```

### 运行所有测试

使用测试脚本（推荐）：

```bash
./test.sh
```

或手动构建和运行：

```bash
mkdir -p build_test && cd build_test
cmake ../src/linkerta -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure
```

## 运行测试

### 基本用法

```bash
# 运行所有测试
./test.sh

# 显示详细输出
./test.sh --verbose

# 生成覆盖率报告
./test.sh --coverage
```

### 运行特定测试

在构建目录中：

```bash
cd build_test
./test/run_tests --gtest_filter=CanBusTest.*
./test/run_tests --gtest_filter=LinkerArmTest.*
```

### 测试选项

Google Test 支持多种命令行选项：

```bash
# 列出所有测试
./test/run_tests --gtest_list_tests

# 运行特定测试
./test/run_tests --gtest_filter=CanBusTest.CANFrameStructure

# 重复运行测试
./test/run_tests --gtest_repeat=10

# 显示详细输出
./test/run_tests --gtest_brief=0
```

## 代码覆盖率

### 生成覆盖率报告

```bash
./test.sh --coverage
```

报告将生成在 `build_test/coverage_html/index.html`，可以在浏览器中打开查看。

### 覆盖率目标

- **目标覆盖率**: > 80%
- **核心功能覆盖率**: > 90%

### 查看覆盖率

```bash
# 打开覆盖率报告
xdg-open build_test/coverage_html/index.html  # Linux
open build_test/coverage_html/index.html       # macOS
```

## 编写测试

### 测试文件结构

测试文件位于 `src/linkerta/test/` 目录：

```
test/
├── CMakeLists.txt          # 测试构建配置
├── test_canbus.cpp         # CanBus 测试
├── test_linkerarm.cpp      # LinkerArm 测试
├── mock_canbus.h           # Mock 对象定义
└── test_main.cpp           # 测试主程序
```

### 编写新测试

1. **创建测试用例**

```cpp
#include <gtest/gtest.h>
#include "YourClass.h"

TEST(YourClassTest, TestName) {
    // 准备测试数据
    YourClass obj;
    
    // 执行测试
    auto result = obj.method();
    
    // 验证结果
    EXPECT_EQ(result, expected_value);
}
```

2. **测试断言**

Google Test 提供多种断言：

```cpp
// 相等性断言
EXPECT_EQ(a, b);      // a == b
EXPECT_NE(a, b);      // a != b

// 浮点数断言
EXPECT_FLOAT_EQ(a, b);    // 近似相等
EXPECT_NEAR(a, b, eps);   // 在误差范围内

// 布尔断言
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// 字符串断言
EXPECT_STREQ(str1, str2);
EXPECT_STRNE(str1, str2);
```

3. **测试夹具**

对于需要共享设置的测试：

```cpp
class YourClassTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    YourClass obj;
};

TEST_F(YourClassTest, TestName) {
    // 使用 obj
}
```

### 测试硬件依赖代码

由于 CanBus 和 LinkerArm 依赖实际的 CAN 硬件，测试策略：

1. **单元测试**: 测试可测试的逻辑部分（数据转换、计算等）
2. **Mock 对象**: 隔离硬件依赖（未来实现）
3. **集成测试**: 使用 vcan（虚拟 CAN）进行集成测试

#### 使用 vcan 进行集成测试

```bash
# 创建虚拟 CAN 接口
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up

# 运行集成测试
./test/run_tests --gtest_filter=*Integration*
```

## 测试结构

### 测试分类

1. **单元测试** (`*Test`)
   - 测试单个函数或方法
   - 不依赖外部资源
   - 快速执行

2. **集成测试** (`*IntegrationTest`)
   - 测试组件间交互
   - 可能需要外部资源（如 vcan）
   - 使用 `DISABLED_` 前缀标记需要特殊环境的测试

### 当前测试覆盖

#### CanBus 测试

- ✅ CAN 帧结构验证
- ✅ Job 结构验证
- ✅ 数据验证和转换
- ✅ CAN ID 验证
- ✅ 数据大小限制
- ⚠️ 硬件相关功能（需要 vcan）

#### LinkerArm 测试

- ✅ 数据结构验证
- ✅ FRAME_PROPERTY 枚举
- ✅ 数据转换逻辑
- ✅ 角度归一化
- ✅ 错误码解析
- ✅ 版本号解析
- ⚠️ 硬件相关功能（需要 vcan）

## 故障排查

### 常见问题

#### 1. 测试编译失败

**问题**: CMake 找不到 Google Test

**解决方案**:
```bash
# 确保网络连接正常（Google Test 会自动下载）
# 或手动指定 GTest 路径
cmake -DGTest_DIR=/path/to/gtest ../src/linkerta
```

#### 2. 测试运行时失败

**问题**: 测试失败但不确定原因

**解决方案**:
```bash
# 使用详细输出
./test.sh --verbose

# 或直接运行测试可执行文件
./build_test/test/run_tests --gtest_brief=0
```

#### 3. 覆盖率报告未生成

**问题**: `make coverage` 失败

**解决方案**:
```bash
# 确保安装了 lcov
sudo apt-get install lcov

# 确保使用 Debug 构建
cmake -DCMAKE_BUILD_TYPE=Debug ../src/linkerta
```

#### 4. 集成测试失败

**问题**: 集成测试需要实际的 CAN 接口

**解决方案**:
```bash
# 使用 vcan（虚拟 CAN）
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up

# 或跳过集成测试
./test/run_tests --gtest_filter=-*Integration*
```

### 调试测试

1. **使用 GDB**

```bash
gdb ./build_test/test/run_tests
(gdb) run --gtest_filter=YourTest.TestName
```

2. **使用 Valgrind** (检测内存问题)

```bash
valgrind --leak-check=full ./build_test/test/run_tests
```

## 持续集成

### GitHub Actions 示例

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake lcov
      - name: Build and test
        run: |
          ./test.sh --coverage
      - name: Upload coverage
        uses: codecov/codecov-action@v2
```

## 贡献指南

添加新功能时，请：

1. ✅ 为新功能编写测试
2. ✅ 确保所有测试通过
3. ✅ 保持或提高代码覆盖率
4. ✅ 更新本文档（如需要）

## 参考资源

- [Google Test 文档](https://google.github.io/googletest/)
- [CMake 测试文档](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [代码覆盖率最佳实践](https://github.com/codecov/example-cpp)
