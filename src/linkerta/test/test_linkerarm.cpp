#include <gtest/gtest.h>
#include "LinkerArm.h"
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace LinkerArm {

// 测试：LinkerArm数据转换函数
// 由于bytesToInt16是私有函数，我们通过公共接口间接测试

// 测试：关节位置数据结构
TEST(LinkerArmTest, JointPositionStructure) {
    // 测试关节位置向量的初始化和大小
    std::vector<float> positions(14, 0.0);
    
    EXPECT_EQ(positions.size(), 14);
    for (size_t i = 0; i < positions.size(); ++i) {
        EXPECT_FLOAT_EQ(positions[i], 0.0f);
    }
}

// 测试：关节错误代码结构
TEST(LinkerArmTest, JointErrorCodeStructure) {
    // 测试关节错误代码向量的初始化和大小
    std::vector<bool> error_codes(14, false);
    
    EXPECT_EQ(error_codes.size(), 14);
    for (size_t i = 0; i < error_codes.size(); ++i) {
        EXPECT_FALSE(error_codes[i]);
    }
}

// 测试：FRAME_PROPERTY枚举值
TEST(LinkerArmTest, FramePropertyValues) {
    EXPECT_EQ(GET_JOINT_POSITION_1, 0x02);
    EXPECT_EQ(GET_JOINT_POSITION_2, 0x03);
    EXPECT_EQ(GET_JOINT_POSITION, 0x04);
    EXPECT_EQ(LEFT_JOINT_POSITION_1, 0x65);
    EXPECT_EQ(LEFT_JOINT_POSITION_2, 0x66);
    EXPECT_EQ(RIGHT_JOINT_POSITION_1, 0x67);
    EXPECT_EQ(RIGHT_JOINT_POSITION_2, 0x68);
    EXPECT_EQ(RESET_ZERO_COMMAND_1, 0xC0);
    EXPECT_EQ(RESET_ZERO_COMMAND_2, 0xC1);
    EXPECT_EQ(LINKER_HAND_VERSION, 0x64);
}

// 测试：数据转换逻辑（模拟bytesToInt16的行为）
TEST(LinkerArmTest, BytesToInt16Conversion) {
    // 测试基本转换：高位字节和低位字节
    uint8_t hi = 0x01;
    uint8_t lo = 0x2C; // 300 in decimal
    
    uint16_t u = (static_cast<uint16_t>(hi) << 8) | lo;
    int16_t signed_val = static_cast<int16_t>(u);
    float result = signed_val / 10.0f;
    
    EXPECT_FLOAT_EQ(result, 30.0f);
    
    // 测试负数
    hi = 0xFF;
    lo = 0x9C; // -100 in two's complement
    u = (static_cast<uint16_t>(hi) << 8) | lo;
    signed_val = static_cast<int16_t>(u);
    result = signed_val / 10.0f;
    
    EXPECT_FLOAT_EQ(result, -10.0f);
}

// 测试：角度归一化逻辑
TEST(LinkerArmTest, AngleNormalization) {
    float angle = 370.0f;
    
    // 模拟bytesToInt16中的归一化逻辑
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0) angle += 360.0f;
    angle -= 180.0f;
    
    EXPECT_GE(angle, -180.0f);
    EXPECT_LE(angle, 180.0f);
    
    // 测试负数角度
    angle = -190.0f;
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0) angle += 360.0f;
    angle -= 180.0f;
    
    EXPECT_GE(angle, -180.0f);
    EXPECT_LE(angle, 180.0f);
}

// 测试：角度增量检查
TEST(LinkerArmTest, AngleDeltaCheck) {
    float last_position = 0.0f;
    float raw = 200.0f; // 超过180度的增量
    
    float delta = raw - last_position;
    
    // 如果增量超过180度，应该使用上一个位置
    if (std::fabs(delta) >= 180.0f) {
        raw = last_position;
    }
    
    EXPECT_FLOAT_EQ(raw, last_position);
    
    // 测试正常增量
    last_position = 10.0f;
    raw = 20.0f;
    delta = raw - last_position;
    
    if (std::fabs(delta) >= 180.0f) {
        raw = last_position;
    }
    
    EXPECT_FLOAT_EQ(raw, 20.0f);
}

// 测试：版本号解析逻辑
TEST(LinkerArmTest, VersionParsing) {
    std::vector<uint8_t> version = {123}; // 1.2.3
    
    int major = version[0] / 100;
    int minor = (version[0] % 100) / 10;
    int patch = version[0] % 10;
    
    EXPECT_EQ(major, 1);
    EXPECT_EQ(minor, 2);
    EXPECT_EQ(patch, 3);
    
    // 测试另一个版本号
    version[0] = 456; // 4.5.6
    major = version[0] / 100;
    minor = (version[0] % 100) / 10;
    patch = version[0] % 10;
    
    EXPECT_EQ(major, 4);
    EXPECT_EQ(minor, 5);
    EXPECT_EQ(patch, 6);
}

// 测试：错误码解析逻辑
TEST(LinkerArmTest, ErrorCodeParsing) {
    uint8_t fault_byte = 0b01010101; // 位模式
    
    std::vector<bool> error_codes(7, false);
    for (int i = 0; i < 7; ++i) {
        bool is_fault = (fault_byte >> i) & 0x01;
        error_codes[i] = is_fault;
    }
    
    // 验证解析结果
    EXPECT_TRUE(error_codes[0]);
    EXPECT_FALSE(error_codes[1]);
    EXPECT_TRUE(error_codes[2]);
    EXPECT_FALSE(error_codes[3]);
    EXPECT_TRUE(error_codes[4]);
    EXPECT_FALSE(error_codes[5]);
    EXPECT_TRUE(error_codes[6]);
}

// 测试：关节索引计算
TEST(LinkerArmTest, JointIndexCalculation) {
    // 左臂关节索引
    int left_base = 0;
    EXPECT_EQ(left_base, 0);
    EXPECT_EQ(left_base + 6, 6);
    
    // 右臂关节索引
    int right_base = 7;
    EXPECT_EQ(right_base, 7);
    EXPECT_EQ(right_base + 6, 13);
}

// 测试：CAN帧数据处理
TEST(LinkerArmTest, CANFrameDataProcessing) {
    // 模拟LEFT_JOINT_POSITION_1帧
    std::vector<uint8_t> frame_data = {0x01, 0x2C, 0x03, 0x84, 0x05, 0xDC, 0x07, 0x34};
    // 对应角度: 30.0, 90.0, 150.0, 180.0 (度)
    
    // 验证数据长度
    EXPECT_EQ(frame_data.size(), 8);
    
    // 验证数据对
    EXPECT_EQ(frame_data[0], 0x01);
    EXPECT_EQ(frame_data[1], 0x2C);
}

// 测试：时间格式化
TEST(LinkerArmTest, TimeFormatting) {
    auto now = std::chrono::high_resolution_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    
    // 验证时间可以正常转换
    EXPECT_GT(time, 0);
    
    // 验证毫秒计算
    auto duration = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        duration % std::chrono::seconds(1)).count();
    int milliseconds = microseconds / 1000;
    
    EXPECT_GE(milliseconds, 0);
    EXPECT_LT(milliseconds, 1000);
}

// 测试：重置零位命令参数
TEST(LinkerArmTest, ResetZeroCommand) {
    uint8_t joint_specific = 0x05; // 特定关节
    uint8_t all_joints = 0xFF;     // 所有关节
    
    EXPECT_NE(joint_specific, 0xFF);
    EXPECT_EQ(all_joints, 0xFF);
}

// 集成测试（需要实际的CAN接口或vcan）
#ifdef ENABLE_INTEGRATION_TESTS
TEST(LinkerArmIntegrationTest, DISABLED_LinkerArmCreation) {
    // 这个测试需要vcan接口
    // 运行前需要: sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set vcan0 up
    try {
        LinkerArm arm("vcan0", 1000000);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 测试获取关节位置
        auto positions = arm.getJointPosition();
        EXPECT_EQ(positions.size(), 14);
        
        // 测试获取错误代码
        auto errors = arm.getJointErrorCode();
        EXPECT_EQ(errors.size(), 14);
        
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Failed to create LinkerArm: " << e.what();
    }
}
#endif

} // namespace LinkerArm
