
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "lw/event.hpp"

namespace lw {
namespace tests {

struct LoopBasicTests : public testing::Test {
    event::Loop loop;
};

// -------------------------------------------------------------------------- //

TEST_F(LoopBasicTests, HelloWorld){
    loop.run();
}

// -------------------------------------------------------------------------- //

TEST_F(LoopBasicTests, IdleLoop){
    const std::uint64_t ticks = 10000;
    std::uint64_t counter = 0;
    event::Idle idle(loop);

    idle.start([&](){
        if (++counter >= ticks) {
            idle.stop();
        }
    });

    EXPECT_EQ(0ul, counter);
    loop.run();
    EXPECT_EQ(ticks, counter);
}

// -------------------------------------------------------------------------- //

TEST_F(LoopBasicTests, Post) {
    int executed = 0;
    loop.post([&]() { ++executed; });

    EXPECT_EQ(0, executed);
    loop.run();
    EXPECT_EQ(1, executed);
}

// -------------------------------------------------------------------------- //

TEST_F(LoopBasicTests, PostThrow) {
    bool caught = false;
    int executed = 0;
    loop.post([&]() {
        ++executed;
        throw "foobar";
    });

    EXPECT_EQ(0, executed);
    try {
        loop.run();
    }
    catch (const char* err) {
        caught = true;
        EXPECT_EQ("foobar", err);
    }
    EXPECT_TRUE(caught);
    EXPECT_EQ(1, executed);
}

// -------------------------------------------------------------------------- //

TEST_F(LoopBasicTests, PostFromThread) {
    int executed = 0;
    std::thread::id loop_thread = std::this_thread::get_id();

    event::Idle idle(loop);
    idle.start([&]() {
        if (executed) {
            idle.stop();
        }
    });

    std::thread background_thread{[&]() {
        EXPECT_NE(loop_thread, std::this_thread::get_id());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        loop.post([&]() {
            EXPECT_EQ(loop_thread, std::this_thread::get_id());
            ++executed;
        });
    }};

    EXPECT_EQ(0, executed);
    loop.run();
    EXPECT_EQ(1, executed);

    if (background_thread.joinable()) {
        background_thread.join();
    }
}

}
}
