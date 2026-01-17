# 测试快速参考

## 快速开始

```bash
# 从项目根目录运行
./test.sh
```

## 测试文件说明

- `test_canbus.cpp` - CanBus 类的单元测试
- `test_linkerarm.cpp` - LinkerArm 类的单元测试
- `mock_canbus.h` - Mock 对象定义（用于未来扩展）
- `test_main.cpp` - 测试主程序入口

## 运行特定测试

```bash
cd build_test
./test/run_tests --gtest_filter=CanBusTest.*
./test/run_tests --gtest_filter=LinkerArmTest.*
```

## 查看测试覆盖率

```bash
./test.sh --coverage
# 然后打开 build_test/coverage_html/index.html
```

## 注意事项

1. 某些测试需要 Linux 环境和 CAN 接口支持
2. 集成测试需要 vcan（虚拟 CAN）接口
3. 硬件相关测试默认被禁用，需要手动启用

详细文档请参考：[测试文档](../../docs/TESTING.md)
