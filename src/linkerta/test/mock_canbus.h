#ifndef MOCK_CANBUS_H
#define MOCK_CANBUS_H

#include <gmock/gmock.h>
#include "CanBus.h"

// 注意：由于CanBus类的方法不是virtual的，我们不能直接继承和mock
// 这个文件保留用于未来可能的接口重构
// 当前测试主要关注可测试的逻辑部分，而不是硬件依赖部分

// 由于CanBus的构造函数会尝试打开真实的CAN设备，我们需要一个测试辅助类
// 这个类可以绕过实际的硬件依赖
namespace TestHelpers {
    // 辅助函数：创建测试用的CAN帧
    inline CANFrame createTestFrame(uint32_t can_id, const std::vector<uint8_t>& data) {
        CANFrame frame;
        frame.can_id = can_id;
        frame.can_dlc = std::min(static_cast<uint8_t>(data.size()), static_cast<uint8_t>(8));
        for (size_t i = 0; i < frame.can_dlc; ++i) {
            frame.data[i] = data[i];
        }
        return frame;
    }
    
    // 辅助函数：从CAN帧提取数据
    inline std::vector<uint8_t> extractData(const CANFrame& frame) {
        return std::vector<uint8_t>(frame.data, frame.data + frame.can_dlc);
    }
}

#endif // MOCK_CANBUS_H
