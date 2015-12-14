
#include <cstdlib>
#include <uv.h>

#include "lw/error.hpp"
#include "lw/io/Pipe.hpp"
#include "lw/io/TCP.hpp"
#include "lw/io/UDP.hpp"
#include "lw/pp.hpp"

namespace lw {
namespace io {

const Pipe::ipc_t Pipe::ipc{};

// ---------------------------------------------------------------------------------------------- //

// The names of UV handle types as they're known in the code.
#define _LW_UV_UC_TYPE_STR(uc, lc) LW_STRINGIFY(LW_CONCAT(UV_, uc)),
static const char* _uv_uc_type_names[] = {
    "UV_UNKNOWN_HANDLE",
    UV_HANDLE_TYPE_MAP(_LW_UV_UC_TYPE_STR)
    "UV_FILE",
    0
};

// The names of UV handle types as they're understood by humans.
#define _LW_UV_LC_TYPE_STR(uc, lc) LW_STRINGIFY(lc),
static const char* _uv_lc_type_names[] = {
    "unknown handle",
    UV_HANDLE_TYPE_MAP(_LW_UV_LC_TYPE_STR)
    "file",
    0
};

// ---------------------------------------------------------------------------------------------- //

Pipe::Pipe(event::Loop& loop):
    event::BasicStream(_make_state(loop, false))
{
    state(state());
}

// ---------------------------------------------------------------------------------------------- //

Pipe::Pipe(event::Loop& loop, const ipc_t&):
    event::BasicStream(_make_state(loop, true))
{
    state(state());
}

// ---------------------------------------------------------------------------------------------- //

void Pipe::open(const int pipe){
    LW_TRACE("Opening pipe " << pipe);
    int res = uv_pipe_open((uv_pipe_t*)lowest_layer(), pipe);
    if (res < 0) {
        throw LW_UV_ERROR(PipeError, res);
    }
}

// ---------------------------------------------------------------------------------------------- //

void Pipe::bind(const std::string& name){
    LW_TRACE("Binding to pipe named \"" << name << "\"");
    int res = uv_pipe_bind((uv_pipe_t*)lowest_layer(), name.c_str());
    if (res < 0) {
        throw LW_UV_ERROR(PipeError, res);
    }
}

// ---------------------------------------------------------------------------------------------- //

event::Future<> Pipe::connect(const std::string& name){
    LW_TRACE("Connecting to pipe named \"" << name << "\"");
    if (m_connect_promise.is_finished() || m_connect_req != nullptr) {
        throw PipeError(1, "Cannot connect a pipe twice.");
    }

    // Set up the connection request.
    m_connect_req = std::shared_ptr<uv_connect_t>(
        (uv_connect_t*)std::malloc(sizeof(uv_connect_t)),
        &std::free
    );
    m_connect_req->data = (void*)this;

    // Start the connection.
    // NOTE: uv_pipe_connect does not return a status code, its sent to the callback.
    uv_pipe_connect(
        m_connect_req.get(),
        (uv_pipe_t*)lowest_layer(),
        name.c_str(),
        [](uv_connect_t* req, int status){
            LW_TRACE("Pipe connection status: " << status);
            Pipe& pipe = *(Pipe*)req->data;
            if (status < 0) {
                pipe.m_connect_promise.reject(LW_UV_ERROR(PipeError, status));
            }
            else {
                pipe.m_connect_promise.resolve();
            }
            pipe.m_connect_req.reset();
        }
    );

    // Return a future.
    return m_connect_promise.future();
}

// ---------------------------------------------------------------------------------------------- //

event::Future<> Pipe::_listen(const int max_backlog){
    LW_TRACE("Listening for connections on pipe " << (void*)state().get());
    int res = uv_listen(lowest_layer(), max_backlog, [](uv_stream_t* handle, int status){
        LW_TRACE("Recieved connection update (" << status << ") for pipe " << (void*)handle->data);

        // Pull out state.
        auto state = std::static_pointer_cast<_PipeState>(
            ((_PipeState*)handle->data)->shared_from_this()
        );
        auto pipe = Pipe(state);

        // Check if we have any pending connections.
        if (!uv_pipe_pending_count((uv_pipe_t*)handle)) {
            LW_TRACE("Received listen callback without any pending connections.");
            return;
        }

        // Check the status.
        if (status < 0) {
            LW_TRACE("Listen callback received error: " << status);
            pipe._close(LW_UV_ERROR(PipeError, status));
            state.reset();
            return;
        }

        // We have a pending connection, lets see what type.
        LW_TRACE("Making client object.");
        std::shared_ptr<BasicStream> client = nullptr;
        uv_handle_type client_type = uv_pipe_pending_type((uv_pipe_t*)handle);
        if (client_type == UV_NAMED_PIPE) {
            client = std::make_shared<io::Pipe>(state->loop);
        }
        else if (client_type == UV_TCP) {
            client = std::make_shared<io::TCP>(state->loop);
        }
        else if (client_type == UV_UDP) {
            client = std::make_shared<io::UDP>(state->loop);
        }
        else {
            LW_TRACE(
                "Unknown client handle type: " << (int)client_type << " "
                << _uv_uc_type_names[client_type] << " (" << _uv_lc_type_names[client_type] << ")"
            );
            throw PipeError((int)client_type, "Unknown client handle type.");
        }

        // Accept the new client.
        LW_TRACE("Accepting client.");
        int res = uv_accept(handle, client->lowest_layer());
        if (res < 0) {
            LW_TRACE("Error accepting handle: " << res);
            throw LW_UV_ERROR(PipeError, res);
        }

        // Pass the client on to the callback.
        LW_TRACE("Passing client to callback.");
        state->listen_callback(client);
    });

    if (res < 0) {
        throw LW_UV_ERROR(PipeError, res);
    }

    return std::static_pointer_cast<_PipeState>(state())->listen_promise.future();
}

// ---------------------------------------------------------------------------------------------- //

void Pipe::_do_close(void){
    // Don't close the pipe if it is already closing.
    if (uv_is_closing((uv_handle_t*)lowest_layer())) {
        return;
    }

    // Pipe isn't already closing, so close it.
    uv_close((uv_handle_t*)lowest_layer(), [](uv_handle_t* handle){
        LW_TRACE("Pipe " << (void*)handle->data << " closed.");
        auto state = std::static_pointer_cast<_PipeState>(
            ((_PipeState*)handle->data)->shared_from_this()
        );

        state->close_promise.resolve();
        state->listen_callback = nullptr;
        state->listen_promise.reset();
        state->close_promise.reset();
    });
}

// ---------------------------------------------------------------------------------------------- //

event::Future<> Pipe::_close(void){
    LW_TRACE("Closing pipe " << (void*)state().get());
    _do_close();

    // Resolve the listen promise.
    auto pipe_state = std::static_pointer_cast<_PipeState>(state());
    pipe_state->listen_promise.resolve();

    return pipe_state->close_promise.future();
}

// ---------------------------------------------------------------------------------------------- //

event::Future<> Pipe::_close(const error::Exception& err){
    LW_TRACE("Closing the pipe with an error: " << err.what() << ".");
    _do_close();

    // Reject the listen promise.
    auto pipe_state = std::static_pointer_cast<_PipeState>(state());
    pipe_state->listen_promise.reject(err);

    return pipe_state->close_promise.future();
}

// ---------------------------------------------------------------------------------------------- //

std::shared_ptr<Pipe::_PipeState> Pipe::_make_state(event::Loop& loop, const bool ipc){
    LW_TRACE("Making pipe state.");
    uv_pipe_t* pipe = (uv_pipe_t*)std::malloc(sizeof(uv_pipe_t));
    uv_pipe_init(loop.lowest_layer(), pipe, ipc);

    // Stick it in a pipe state handle too.
    auto state_ptr = std::make_shared<_PipeState>(loop);
    state_ptr->handle = (uv_stream_t*)pipe;
    LW_TRACE("Made state " << (void*)state_ptr.get());
    return state_ptr;
}

}
}
