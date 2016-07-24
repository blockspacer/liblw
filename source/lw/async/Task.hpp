#pragma once

#include <memory>
#include <thread>

#include "lw/error.hpp"
#include "lw/event.hpp"
#include "lw/trait.hpp"

namespace lw {
namespace async {

template<typename Result, typename Func>
class Task;

namespace _details {
    template<typename Result>
    struct ExecTask {
        typedef Result result_type;
        typedef event::Promise<Result> promise_type;
        typedef typename promise_type::future_type future_type;

        template <typename Func, typename... Args>
        static future_type operate(Task<Result, Func>& task, Args&&... args) {
            std::shared_ptr<promise_type> promise{new promise_type()};
            std::shared_ptr<std::thread> thread{new std::thread()};
            *thread = std::thread([
                state = task.m_state,
                thread,
                promise,
                args_tuple{std::forward_as_tuple(std::forward<Args>(args)...)}
            ]() {
                try {
                    // Attempt to post the result directly to the loop. If the
                    // func throws, the post call will not be made.
                    state->loop.post([
                        thread,
                        promise,
                        res{trait::apply(args_tuple, state->func)}
                    ]() {
                        if (thread->joinable()) {
                            thread->join();
                        }
                        promise->resolve(std::move(res));
                    });
                }
                catch (const error::Exception& err) {
                    // In case of error, post to the main loop and then reject
                    // the promise.
                    state->loop.post([thread, promise, err]() {
                        if (thread->joinable()) {
                            thread->join();
                        }
                        promise->reject(err);
                    });
                }
            });

            return promise->future();
        }
    };

    // ---------------------------------------------------------------------- //

    template<>
    struct ExecTask<void> {
        typedef void result_type;
        typedef event::Promise<void> promise_type;
        typedef typename promise_type::future_type future_type;

        template <typename Func, typename... Args>
        static future_type operate(Task<void, Func>& task, Args&&... args) {
            std::shared_ptr<promise_type> promise{new promise_type()};
            std::shared_ptr<std::thread> thread{new std::thread()};
            *thread = std::thread([
                state = task.m_state,
                thread,
                promise,
                args_tuple{std::forward_as_tuple(std::forward<Args>(args)...)}
            ]() {
                try {
                    // Execute the function and then post the resolution to the
                    // event loop. Since this is for void types, we don't need
                    // to pass the result through.
                    trait::apply(args_tuple, state->func);
                    state->loop.post([thread, promise]() {
                        if (thread->joinable()) {
                            thread->join();
                        }
                        promise->resolve();
                    });
                }
                catch (const error::Exception& err) {
                    // In case of error, post to the main loop and then reject
                    // the promise.
                    state->loop.post([thread, promise, err]() {
                        if (thread->joinable()) {
                            thread->join();
                        }
                        promise->reject(err);
                    });
                }
            });

            return promise->future();
        }
    };
}

template <typename Result, typename Func>
class Task {
public:
    typedef Result result_type;

    Task(const Task& other) = default;
    Task(Task&& other) = default;

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
        return _details::ExecTask<Result>::operate(*this, std::forward<Args>(args)...);
    }

private:
    friend struct _details::ExecTask<Result>;
    struct _State {
        event::Loop& loop;
        Func func;
    };

    std::shared_ptr<_State> m_state;
};

// -------------------------------------------------------------------------- //

/// @brief Makes a task spawning object.
///
/// The returned functor, when called, will execute the function in a background
/// thread and then make the results available within the given loop.
///
/// @tparam Result  The type of the return value of `Func`.
/// @tparam Func    The type of the functor to make into a task.
///
/// @param loop The loop where task results will be posted.
/// @param func The functor to turn into a task.
///
/// @return A new functor which, when executed, will call `func` in a background
///     thread. The return value from `func` will be posted to the event loop.
template<typename Result, typename Func>
Task<Result, Func> make_task(event::Loop& loop, Func&& func) {
    return Task<Result, Func>(loop, std::forward<Func>(func));
}

template<typename Func>
typename event::FutureResultOf<Func()>::type execute(event::Loop& loop, Func&& func) {
    return make_task(loop, std::forward<Func>(func))();
}

}
}
