#pragma once

#include <memory>
#include <thread>

#include "lw/error.hpp"
#include "lw/event.hpp"
#include "lw/trait.hpp"

namespace lw {
namespace async {

template <typename Result, typename Func>
class Task {
public:
    typedef Result result_type;

    template <typename UFunc>
    Task(event::Loop& loop, UFunc&& func):
        m_state(new _State{loop, std::forward<UFunc>(func)})
    {}

    template <typename... Args>
    event::Future<result_type> operator()(Args&&... args) {
        static_assert(
            trait::is_callable<Func(Args...)>::value,
            "Arguments to operator() must be compatible with Func."
        );

        std::shared_ptr<event::Promise<result_type>> promise{
            new event::Promise<result_type>()
        };

        std::shared_ptr<std::thread> thread{new std::thread()};
        *thread = std::thread([
            state = m_state,
            thread,
            promise,
            args_tuple{std::forward_as_tuple(std::forward<Args>(args)...)}
        ]() {
            try {
                // Attempt to post the result directly to the loop. If the func
                // throws, the post call will not be made.
                state->loop.post([
                    thread,
                    promise,
                    res{trait::apply(args_tuple, state->func)}
                ]() mutable {
                    if (thread->joinable()) {
                        thread->join();
                    }
                    promise->resolve(std::move(res));
                });
            }
            catch (const error::Exception& err) {
                // In case of error, post to the main loop and then reject the
                // promise.
                state->loop.post([thread, promise, err]() mutable {
                    if (thread->joinable()) {
                        thread->join();
                    }
                    promise->reject(err);
                });
            }
        });

        return promise->future();
    }

private:
    struct _State {
        event::Loop& loop;
        Func func;
    };

    std::shared_ptr<_State> m_state;
};

// -------------------------------------------------------------------------- //

template <typename Func>
class Task<void, Func> {
public:
    typedef void result_type;

    template <typename UFunc>
    Task(event::Loop& loop, UFunc&& func):
        m_state(new _State{loop, std::forward<UFunc>(func)})
    {}

    template <typename... Args>
    event::Future<result_type> operator()(Args&&... args) {
        static_assert(
            trait::is_callable<Func(Args...)>::value,
            "Arguments to operator() must be compatible with Func."
        );

        std::shared_ptr<event::Promise<result_type>> promise{
            new event::Promise<result_type>()
        };

        std::shared_ptr<std::thread> thread{new std::thread()};
        *thread = std::thread([
            state = m_state,
            thread,
            promise,
            args_tuple{std::forward_as_tuple(std::forward<Args>(args)...)}
        ]() {
            try {
                // Attempt to post the result directly to the loop. If the func
                // throws, the post call will not be made.
                trait::apply(args_tuple, state->func);
                state->loop.post([thread, promise]() mutable {
                    if (thread->joinable()) {
                        thread->join();
                    }
                    promise->resolve();
                });
            }
            catch (const error::Exception& err) {
                // In case of error, post to the main loop and then reject the
                // promise.
                state->loop.post([thread, promise, err]() mutable {
                    if (thread->joinable()) {
                        thread->join();
                    }
                    promise->reject(err);
                });
            }
        });

        return promise->future();
    }

private:
    struct _State {
        event::Loop& loop;
        Func func;
    };

    std::shared_ptr<_State> m_state;
};

}
}
