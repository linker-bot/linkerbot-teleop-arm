#include <gtest/gtest.h>
#include <iostream>

// 测试主程序入口
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=========================================" << std::endl;
    std::cout << "LinkerArm Unit Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    std::cout << "\n=========================================" << std::endl;
    if (result == 0) {
        std::cout << "All tests passed!" << std::endl;
    } else {
        std::cout << "Some tests failed." << std::endl;
    }
    std::cout << "=========================================" << std::endl;
    
    return result;
}
