#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // std::jthread (C++20): auto-joins on destruction, supports cooperative stop.
    std::jthread worker([](std::stop_token stoken) {
        int count = 0;
        while (!stoken.stop_requested()) {
            std::cout << "working... " << count++ << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "stop requested — worker exiting cleanly\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    std::cout << "main: leaving scope (jthread will request stop + join)\n";
    // No manual join() or stop() needed — the destructor handles both.
}
