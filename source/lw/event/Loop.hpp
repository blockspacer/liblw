#pragma once

#include <functional>

#include "lw/trait.hpp"

struct uv_loop_s;

namespace lw {
namespace event {

/// @brief The event loop which runs all tasks.
class Loop {
public:
    /// @brief Default constructor.
    Loop(void);

    /// @brief No copying.
    Loop(const Loop&) = delete;

    /// @brief Move constructor.
    Loop(Loop&& other):
        m_loop(other.m_loop)
    {}

    // ---------------------------------------------------------------------- //

    /// @brief Extendable destructor.
    virtual ~Loop(void);

    // ---------------------------------------------------------------------- //

    /// @brief Runs all tasks in the loop.
    ///
    /// As long as there are items scheduled on the event loop, this method will
    /// not return. Once all tasks complete, and there are no connections
    /// keeping the loop alive, this method will return.
    void run(void);

    // ---------------------------------------------------------------------- //

    /// @brief Gives access to the native loop handle.
    uv_loop_s* lowest_layer(void){
        return m_loop;
    }

    // ---------------------------------------------------------------------- //

    /// @brief Executes a task on the event loop.
    ///
    /// This function can be called from any thread and the task will be posted
    /// safely to this loop's execution thread.
    ///
    /// @tparam Func The type of the functor to be executed.
    ///
    /// @param func The function to execute on the event loop.
    template<
        typename Func,
        typename = typename std::enable_if<trait::is_callable<Func()>::value>::type
    >
    void post(Func&& func) {
        _post(std::function<void()>(std::forward<Func>(func)));
    }

    // ---------------------------------------------------------------------- //
private:
    uv_loop_s* m_loop;

    void _post(std::function<void()>&& func);
};

}
}
