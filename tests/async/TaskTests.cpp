
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "lw/async.hpp"
#include "lw/event.hpp"

namespace lw {
namespace tests {

struct TaskTests : public testing::Test {
    int task_executed = 0;
    int then_executed = 0;
    std::thread::id loop_thread = std::this_thread::get_id();

    event::Loop loop;
    event::Idle idle{loop};

    TaskTests() {
        idle.start([&]() {
            if (task_executed && then_executed) {
                idle.stop();
            }
        });
    }
};

// -------------------------------------------------------------------------- //

TEST_F(TaskTests, Execute_Void_Void) {
    auto task = async::make_task<void>(loop, [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ++task_executed;
        EXPECT_NE(loop_thread, std::this_thread::get_id());
    });

    task().then([&]() {
        ++then_executed;
        EXPECT_EQ(loop_thread, std::this_thread::get_id());
    });

    EXPECT_EQ(0, task_executed);
    EXPECT_EQ(0, then_executed);
    loop.run();
    EXPECT_EQ(1, task_executed);
    EXPECT_EQ(1, then_executed);
}

// -------------------------------------------------------------------------- //

TEST_F(TaskTests, Execute_Void_Int) {
    auto task = async::make_task<void>(loop, [&](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ++task_executed;
        EXPECT_NE(loop_thread, std::this_thread::get_id());
        EXPECT_EQ(4, i);
    });

    task(4).then([&]() {
        ++then_executed;
        EXPECT_EQ(loop_thread, std::this_thread::get_id());
    });

    EXPECT_EQ(0, task_executed);
    EXPECT_EQ(0, then_executed);
    loop.run();
    EXPECT_EQ(1, task_executed);
    EXPECT_EQ(1, then_executed);
}

// -------------------------------------------------------------------------- //

TEST_F(TaskTests, Execute_Int_Void) {
    auto task = async::make_task<int>(loop, [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ++task_executed;
        EXPECT_NE(loop_thread, std::this_thread::get_id());
        return 8;
    });

    task().then([&](int res) {
        ++then_executed;
        EXPECT_EQ(loop_thread, std::this_thread::get_id());
        EXPECT_EQ(8, res);
    });

    EXPECT_EQ(0, task_executed);
    EXPECT_EQ(0, then_executed);
    loop.run();
    EXPECT_EQ(1, task_executed);
    EXPECT_EQ(1, then_executed);
}

// -------------------------------------------------------------------------- //

TEST_F(TaskTests, Execute_Int_Int) {
    auto task = async::make_task<int>(loop, [&](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ++task_executed;
        EXPECT_NE(loop_thread, std::this_thread::get_id());
        EXPECT_EQ(4, i);
        return i * 2;
    });

    task(4).then([&](int res) {
        ++then_executed;
        EXPECT_EQ(loop_thread, std::this_thread::get_id());
        EXPECT_EQ(8, res);
    });

    EXPECT_EQ(0, task_executed);
    EXPECT_EQ(0, then_executed);
    loop.run();
    EXPECT_EQ(1, task_executed);
    EXPECT_EQ(1, then_executed);
}

}
}
