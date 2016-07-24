
#include <cstdlib>
#include <exception>
#include <uv.h>

#include "lw/event/Loop.hpp"

namespace lw {
namespace event {

Loop::Loop(void):
    m_loop((uv_loop_s*)std::malloc(sizeof(uv_loop_s)))
{
    uv_loop_init(m_loop);
}

// -------------------------------------------------------------------------- //

Loop::~Loop(void) {
    uv_loop_close(m_loop);
    std::free(m_loop);
}

// -------------------------------------------------------------------------- //

void Loop::run(void) {
    uv_run(m_loop, UV_RUN_DEFAULT);
}

// -------------------------------------------------------------------------- //

void Loop::_post(std::function<void()>&& func) {
    typedef std::function<void()> func_type;

    uv_async_t* async = (uv_async_t*)malloc(sizeof(uv_async_t));
    uv_async_init(lowest_layer(), async, [](uv_async_t* async) {
        func_type* func = (func_type*)async->data;
        std::exception_ptr err;

        // Try executing the function.
        try {
            (*func)();
        }
        catch (...) {
            err = std::current_exception();
        }

        // Cleanup the resources for this.
        uv_close((uv_handle_t*)async, [](uv_handle_t* async) {
            delete (func_type*)async->data;
            free((uv_async_t*)async);
        });

        // If the functor errored, rethrow that.
        if (err) {
            std::rethrow_exception(err);
        }
    });

    async->data = new func_type(std::move(func));
    uv_async_send(async);
}

}
}
