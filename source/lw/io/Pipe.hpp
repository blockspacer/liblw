#pragma once

#include "lw/event.hpp"

struct uv_connect_s;

namespace lw {
namespace io {

LW_DEFINE_EXCEPTION_EX(PipeError, ::lw::event, StreamError);

// ---------------------------------------------------------------------------------------------- //

/// @brief A local process-to-process pipe.
class Pipe : public event::BasicStream {
public:
    struct ipc_t {};
    static const ipc_t ipc;

    /// @brief Well known pipe descriptors.
    enum descriptors {
        IN  = 0,    ///< stdin
        OUT = 1,    ///< stdout
        ERR = 2     ///< stderr
    };

    /// @brief
    ///
    /// @param client
    typedef std::function<void(const std::shared_ptr<event::BasicStream>& client)> listen_callback_t;

    // ------------------------------------------------------------------------------------------ //

    /// @brief Constructs a standard pipe.
    ///
    /// @param loop The even loop for the pipe.
    Pipe(event::Loop& loop);

    /// @brief Constructs a pipe that can be used to pass handles.
    ///
    /// @param loop The even loop for the pipe.
    Pipe(event::Loop& loop, const ipc_t&);

    // ------------------------------------------------------------------------------------------ //

    /// @brief Opens a pipe on an existing pipe descriptor.
    ///
    /// @param pipe The pipe descriptor to open.
    void open(const int pipe);

    /// @brief Creates a new named pipe/Unix socket and sets this process as the owner.
    ///
    /// @param name The name of the pipe, such as `/tmp/my-awesome-pipe`.
    void bind(const std::string& name);

    /// @brief Connects to an existing named pipe/Unix socket.
    ///
    /// @param name The name of the pipe to connect to.
    ///
    /// @return A promise to be connected.
    event::Future<> connect(const std::string& name);

    // ------------------------------------------------------------------------------------------ //

    template<typename Func>
    event::Future<> listen(const int max_backlog, Func&& cb){
        static_assert(
            std::is_convertible<Func, listen_callback_t>::value,
            "`Func` must be compatible with `io::Pipe::listen_callback_t`."
        );
        auto pipe_state = std::static_pointer_cast<_PipeState>(state());
        pipe_state->listen_callback =
            [pipe_state, cb](const std::shared_ptr<event::BasicStream>& client){ cb(client); };
        return _listen(max_backlog);
    }

    template<typename Func>
    event::Future<> listen(Func&& cb){
        return listen(128, std::forward<Func>(cb));
    }

    // ------------------------------------------------------------------------------------------ //

    event::Future<> close(void){
        return _close();
    }

    // ------------------------------------------------------------------------------------------ //

private:
    struct _PipeState : public BasicStream::_State {
        _PipeState(event::Loop& _loop):
            loop(_loop)
        {}

        event::Loop& loop;
        listen_callback_t listen_callback;
        event::Promise<> listen_promise;
        event::Promise<> close_promise;
    };

    /// @brief Constructs the internal shared state for the pipe.
    ///
    /// @param loop The event loop that the pipe will use.
    /// @param ipc  Flag indicating if this pipe will be used to pass handles.
    ///
    /// @return A new shared state for a pipe.
    static std::shared_ptr<_PipeState> _make_state(event::Loop& loop, const bool ipc);

    // ------------------------------------------------------------------------------------------ //

    Pipe(const std::shared_ptr<_PipeState>& state):
        BasicStream(state)
    {}

    // ------------------------------------------------------------------------------------------ //

    event::Future<> _listen(const int max_backlog);

    // ------------------------------------------------------------------------------------------ //

    event::Future<> _close(void);

    event::Future<> _close(const error::Exception& err);

    // ------------------------------------------------------------------------------------------ //

    event::Promise<> m_connect_promise;             ///< Promise for making connections.
    std::shared_ptr<uv_connect_s> m_connect_req;    ///< Connection request handle.
};

}
}
