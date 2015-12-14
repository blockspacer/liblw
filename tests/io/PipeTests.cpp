
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
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
    const std::string pipe_name     = "/tmp/liblw_pipe_tests_named_pipe";
    memory::Buffer contents;

    PipeTests(void):
        contents(content_str.size())
    {
        contents.copy(content_str.begin(), content_str.end());
    }

    void SetUp(void) override {
        ::pipe(pipe_descriptors);
        ::remove(pipe_name.c_str());
    }

    void TearDown(void) override {
        ::close(pipe_descriptors[0]);
        ::close(pipe_descriptors[1]);
        ::remove(pipe_name.c_str());
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

        return event::wait(loop, 100ms).then([&](){
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

TEST_F(PipeTests, BindRead){
    io::Pipe pipe(loop, io::Pipe::ipc);
    bool started = false;
    bool finished = false;
    bool received_client = false;
    bool promise_called = false;
    bool listen_completed = false;

    // Set up the pipe to read.
    pipe.bind(pipe_name);
    pipe.listen([&](const std::shared_ptr<event::BasicStream>& client){
        LW_TRACE("Received a connection, accepting.");
        EXPECT_TRUE(started);
        EXPECT_FALSE(received_client);
        received_client = true;

        client->read([&](const std::shared_ptr<const memory::Buffer>& buffer){
            LW_TRACE("Read " << buffer->size() << " bytes.");
            EXPECT_TRUE(started);
            EXPECT_FALSE(finished);
            EXPECT_EQ(contents, *buffer);
        }).then([&](const std::size_t bytes_read){
            LW_TRACE("Read completed, " << bytes_read << " total bytes read.");
            promise_called = true;
            EXPECT_TRUE(started);
            EXPECT_FALSE(finished);
            EXPECT_EQ(contents.size(), bytes_read);
        });
    }).then([&](){
        LW_TRACE("Listening stopped.");
        EXPECT_TRUE(received_client);
        EXPECT_FALSE(listen_completed);
        listen_completed = true;
    });

    // Separately, create a timeout that will send the data on the pipe once the loop has started.
    event::wait(loop, 10ms).then([&](){
        LW_TRACE("Delayed connecting to pipe.");

        // Prepare the socket structure.
        auto sock = std::make_shared<sockaddr_un>();
        sock->sun_family = AF_UNIX;
        std::strcpy(sock->sun_path, pipe_name.c_str());
        int sock_size = sizeof(sock->sun_family) + pipe_name.size() + 1;

        // Open the socket and connect!
        int sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        EXPECT_NE(-1, sock_fd) << ::strerror(errno);
        int res = ::connect(sock_fd, (sockaddr*)sock.get(), sock_size);
        EXPECT_NE(-1, res) << ::strerror(errno);

        return event::wait(loop, 10ms).then([&, sock_fd](){
            LW_TRACE("Delayed writing to pipe.");
            ::send(sock_fd, content_str.c_str(), content_str.size(), 0);

            return event::wait(loop, 10ms);
        }).then([&, sock_fd, sock](){
            LW_TRACE("Delayed closing of pipe.");
            ::shutdown(sock_fd, SHUT_RDWR);

            return event::wait(loop, 10ms);
        });
    }).then([&](){
        LW_TRACE("Delayed closing of io::Pipe");
        return pipe.close();
    }).then([&](){
        LW_TRACE("Pipe closed.");
    });

    started = true;
    loop.run();
    finished = true;

    EXPECT_TRUE(received_client);
    EXPECT_TRUE(promise_called);
    EXPECT_TRUE(listen_completed);
}

}
}
