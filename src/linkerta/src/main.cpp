// main.cpp
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>

#include "LinkerArm.h"

using namespace std;

static atomic<bool> g_running{true};

void signal_handler(int)
{
    g_running = false;
}

int main(int argc, char *argv[])
{
    /* 1. 解析命令行参数，默认用 can0 1Mbps */
    string can_if  = (argc > 1) ? argv[1] : "can0";
    int baudrate   = (argc > 2) ? stoi(argv[2]) : 1000000;
    uint32_t hand_id = (argc > 3) ? stoul(argv[3], nullptr, 16) : 0x123;

    /* 2. 注册 Ctrl-C 退出 */
    signal(SIGINT, signal_handler);

    try
    {
        /* 3. 创建机械臂通信对象 */
        LinkerArm::LinkerArm arm("can0", 1000000);
        this_thread::sleep_for(200ms);          // 等接收线程起来

        /* 4. 主循环：1 Hz 发 GET_JOINT_POSITION */
        while (g_running)
        {
            std::vector<float> temp = arm.getJointPosition();             // 发送 0x01 命令
            std::cout << "..." << std::endl;
            this_thread::sleep_for(1s);
        }
    }
    catch (const exception &e)
    {
        cerr << "[ERROR] " << e.what() << endl;
        return -1;
    }

    cout << "main exit cleanly." << endl;
    return 0;
}
