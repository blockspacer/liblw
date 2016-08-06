
#include <gtest/gtest.h>

#include "lw/event.hpp"

namespace lw {
namespace tests {

struct PromiseTraitsTests : public testing::Test {};

// -------------------------------------------------------------------------- //

template <typename T>
class TemplateSubclass : public event::Future<T> {};

class Subclass : public event::Future<int> {};

template <typename T, typename U>
testing::AssertionResult same_type(const char* t_name, const char* u_name) {
    if (std::is_same<T, U>::value) {
        return testing::AssertionSuccess() << t_name << " is same as " << u_name;
    }
    else {
        return testing::AssertionFailure() << t_name << " is not " << u_name;
    }
}

#define EXPECT_SAME_TYPE(T, U) EXPECT_TRUE((same_type<T, U>(#T, #U)))

// -------------------------------------------------------------------------- //

TEST_F(PromiseTraitsTests, IsFuture) {
    EXPECT_TRUE(event::IsFuture<event::Future<>>::value);
    EXPECT_TRUE(event::IsFuture<event::Future<int>>::value);
    EXPECT_TRUE(event::IsFuture<TemplateSubclass<int>>::value);
    EXPECT_TRUE(event::IsFuture<Subclass>::value);

    EXPECT_FALSE(event::IsFuture<void>::value);
    EXPECT_FALSE(event::IsFuture<int>::value);
    EXPECT_FALSE(event::IsFuture<PromiseTraitsTests>::value);
}

// -------------------------------------------------------------------------- //

TEST_F(PromiseTraitsTests, UnwrapFuture) {
    EXPECT_SAME_TYPE(event::UnwrapFuture<event::Future<>>::result_type, void);
    EXPECT_SAME_TYPE(event::UnwrapFuture<event::Future<int>>::result_type, int);
    EXPECT_SAME_TYPE(event::UnwrapFuture<TemplateSubclass<int>>::result_type, int);
    EXPECT_SAME_TYPE(event::UnwrapFuture<Subclass>::result_type, int);
    EXPECT_SAME_TYPE(event::UnwrapFuture<void>::result_type, void);
    EXPECT_SAME_TYPE(event::UnwrapFuture<int>::result_type, int);
    EXPECT_SAME_TYPE(event::UnwrapFuture<PromiseTraitsTests>::result_type, PromiseTraitsTests);

    EXPECT_SAME_TYPE(event::UnwrapFuture<event::Future<>>::type, event::Future<void>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<event::Future<int>>::type, event::Future<int>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<TemplateSubclass<int>>::type, event::Future<int>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<Subclass>::type, event::Future<int>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<void>::type, event::Future<void>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<int>::type, event::Future<int>);
    EXPECT_SAME_TYPE(event::UnwrapFuture<PromiseTraitsTests>::type, event::Future<PromiseTraitsTests>);
}

}
}
