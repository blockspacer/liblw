
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <unistd.h>

#include "lw/event.hpp"
#include "lw/io.hpp"
#include "lw/memory.hpp"
#include "lw/pp.hpp"

using namespace std::chrono_literals;

namespace lw {
namespace tests {

struct PipeTests : public testing::Test {
    event::Loop loop;
    int pipe_descriptors[2];
    const std::string content_str   = "an awesome message to keep";
    const std::string pipe_name     = "liblw_pipe_tests_named_pipe";
    memory::Buffer contents;

    PipeTests(void):
        contents(content_str.size())
    {
        contents.copy(content_str.begin(), content_str.end());
    }

    void SetUp(void) override {
        ::pipe(pipe_descriptors);
    }

    void TearDown(void) override {
        ::close(pipe_descriptors[0]);
        ::close(pipe_descriptors[1]);
    }
};

// ---------------------------------------------------------------------------------------------- //

TEST_F(PipeTests, Read){
    io::Pipe pipe(loop);
    bool started = false;
    bool buffer_received = false;
    bool finished = false;
    bool promise_called = false;

    // Set up the pipe to read.
    pipe.open(pipe_descriptors[0]);
    pipe.read([&](const std::shared_ptr<const memory::Buffer>& buffer){
        LW_TRACE("Read callback.");
        SCOPED_TRACE("Read callback.");

        buffer_received = true;
        EXPECT_TRUE(started);
        EXPECT_FALSE(finished);
        EXPECT_EQ(contents, *buffer);
    }).then([&](const std::size_t bytes_read){
        LW_TRACE("Read completion callback.");
        SCOPED_TRACE("Read completion callback.");

        promise_called = true;
        EXPECT_TRUE(started);
        EXPECT_TRUE(buffer_received);
        EXPECT_FALSE(finished);
        EXPECT_LT( 0, bytes_read );
        EXPECT_EQ(contents.size(), bytes_read);
    });

    // Separately, create a timeout that will send the data on the pipe once the loop has started.
    event::wait(loop, 0s).then([&](){
        LW_TRACE("Delayed write of " << content_str.size() << " bytes: \"" << content_str << "\".");
        SCOPED_TRACE("Delayed write.");
        ::write(pipe_descriptors[1], content_str.c_str(), content_str.size());
        ::fsync(pipe_descriptors[1]);

        return event::wait(loop, 500ms).then([&](){
            LW_TRACE("Delayed closing of pipe.");
            SCOPED_TRACE("Delayed closing of pipe.");
            ::close(pipe_descriptors[1]);
        });
    });

    LW_TRACE("Loop starting.");
    started = true;
    loop.run();
    finished = true;
    LW_TRACE("Loop completed.");

    EXPECT_TRUE(promise_called);
}

// ---------------------------------------------------------------------------------------------- //

TEST_F(PipeTests, StopRead){
    io::Pipe pipe(loop);
    bool started = false;
    bool finished = false;
    bool promise_called = false;

    // Set up the pipe to read.
    pipe.open(pipe_descriptors[0]);
    pipe.read([&](const std::shared_ptr<const memory::Buffer>& buffer){
        EXPECT_TRUE(started);
        EXPECT_FALSE(finished);
        EXPECT_EQ(contents, *buffer);

        pipe.stop_read();
    }).then([&](const std::size_t bytes_read){
        promise_called = true;
        EXPECT_TRUE(started);
        EXPECT_FALSE(finished);
        EXPECT_EQ(contents.size(), bytes_read);
    });

    // Separately, create a timeout that will send the data on the pipe once the loop has started.
    event::wait(loop, 0s).then([&](){
        ::write(pipe_descriptors[1], content_str.c_str(), content_str.size());
        ::fsync(pipe_descriptors[1]);
        // No close here, should stop on its own.
    });

    started = true;
    loop.run();
    finished = true;

    EXPECT_TRUE(promise_called);
}

// ---------------------------------------------------------------------------------------------- //

TEST_F(PipeTests, Write){
    io::Pipe pipe(loop);
    bool started = false;
    bool finished = false;
    bool promise_called = false;

    std::shared_ptr<memory::Buffer> data(&contents, [](memory::Buffer*){});

    // Set up the pipe to write.
    pipe.open(pipe_descriptors[1]);
    pipe.write(data).then([&](const std::size_t bytes_written){
        promise_called = true;
        EXPECT_TRUE(started);
        EXPECT_FALSE(finished);
        EXPECT_EQ(contents.size(), bytes_written);
    });

    // Separately, create a timeout that read the data from the pipe once the loop has started.
    event::wait(loop, 0s).then([&](){
        memory::Buffer buffer(1024);
        int bytes_read = ::read(pipe_descriptors[0], buffer.data(), buffer.capacity());

        EXPECT_EQ((int)contents.size(), bytes_read);
        EXPECT_EQ(contents, memory::Buffer(buffer.data(), bytes_read));
    });

    started = true;
    loop.run();
    finished = true;

    EXPECT_TRUE(promise_called);
}

// ---------------------------------------------------------------------------------------------- //

// TEST_F(PipeTests, BindRead){
//     io::Pipe pipe(loop);
//     bool started = false;
//     bool finished = false;
//     bool promise_called = false;
//
//     // Set up the pipe to read.
//     pipe.bind(pipe_name);
//     pipe.read([&](const std::shared_ptr<const memory::Buffer>& buffer){
//         EXPECT_TRUE(started);
//         EXPECT_FALSE(finished);
//         EXPECT_EQ(contents, *buffer);
//     }).then([&](const std::size_t bytes_read){
//         promise_called = true;
//         EXPECT_TRUE(started);
//         EXPECT_FALSE(finished);
//         EXPECT_EQ(contents.size(), bytes_read);
//     });
//
//     // Separately, create a timeout that will send the data on the pipe once the loop has started.
//     event::wait(loop, 0s).then([&](){
//         int pipe_fd = ::open(pipe_name.c_str(), O_WRONLY);
//         ::write(pipe_fd, content_str.c_str(), content_str.size());
//         ::fsync(pipe_fd);
//         ::close(pipe_fd);
//     });
//
//     started = true;
//     loop.run();
//     finished = true;
//
//     EXPECT_TRUE(promise_called);
// }

}
}
