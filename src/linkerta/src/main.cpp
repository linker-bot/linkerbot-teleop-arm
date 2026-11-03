// main.cpp
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <thread>

#include "LinkerArm.h"

using namespace std;

static volatile sig_atomic_t g_running = 1;

void signal_handler(int) {
    g_running = 0;
}

int main(int argc, char *argv[]) {
    /* 2. 注册 Ctrl-C 退出 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        if (argc > 3)
            throw invalid_argument("usage: linkerta [interface] [bitrate]");
        const string can_if = argc > 1 ? argv[1] : "can0";
        size_t consumed = 0;
        const string rate_text = argc > 2 ? argv[2] : "1000000";
        const int baudrate = stoi(rate_text, &consumed);
        if (consumed != rate_text.size() || baudrate <= 0)
            throw invalid_argument("bitrate must be a positive integer");
        /* 3. 创建机械臂通信对象 */
        LinkerArm::LinkerArm arm(can_if, baudrate);
        this_thread::sleep_for(200ms); // 等接收线程起来

        /* 4. 主循环：1 Hz 发 GET_JOINT_POSITION */
        while (g_running) {
            std::vector<float> temp = arm.getJointPosition(); // 发送 0x01 命令
            for (const auto value : temp)
                std::cout << value << " ";
            std::cout << std::endl;
            this_thread::sleep_for(1s);
        }
    } catch (const exception &e) {
        cerr << "[ERROR] " << e.what() << endl;
        return -1;
    }

    cout << "main exit cleanly." << endl;
    return 0;
}
