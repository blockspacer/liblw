
#include <chrono>
#include <gtest/gtest.h>

#include "lw/event.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

namespace lw {
namespace tests {

struct TimeoutTests : public testing::Test {
    typedef high_resolution_clock clock;
    typedef clock::time_point time_point;

    static const milliseconds short_delay;
    static const milliseconds repeat_interval;
    static const milliseconds max_discrepancy;

    event::Loop loop;
};

const milliseconds TimeoutTests::short_delay        = 25ms;
const milliseconds TimeoutTests::repeat_interval    =  5ms;
const milliseconds TimeoutTests::max_discrepancy    =  3ms;

// ---------------------------------------------------------------------------------------------- //

TEST_F( TimeoutTests, NoDelay ){
    time_point start;
    bool resolved = false;

    event::Timeout timeout( loop );
    timeout.start( 0s ).then([&](){
        EXPECT_LT( clock::now() - start, max_discrepancy );

        resolved = true;
    });
    EXPECT_FALSE( resolved );

    start = clock::now();
    loop.run();
    EXPECT_TRUE( resolved );
}

// ---------------------------------------------------------------------------------------------- //

TEST_F(TimeoutTests, ShortDelay){
    time_point start;
    bool resolved = false;

    event::Timeout timeout(loop);
    timeout.start(short_delay).then([&](){
        auto time_passed = duration_cast<milliseconds>(clock::now() - start);
        EXPECT_LE(time_passed, short_delay + max_discrepancy);
        EXPECT_GE(time_passed, short_delay - max_discrepancy);

        resolved = true;
    });
    EXPECT_FALSE(resolved);

    start = clock::now();
    loop.run();
    EXPECT_TRUE(resolved);
}

// ---------------------------------------------------------------------------------------------- //

TEST_F( TimeoutTests, Repeat ){
    time_point start;
    time_point previous_call;
    int call_count = 0;
    bool resolved = false;

    event::Timeout timeout( loop );
    timeout.repeat( repeat_interval, [&]( event::Timeout& repeat_timeout ){
        ++call_count;
        time_point call_time = clock::now();

        { // Check the total delay since starting.
            auto time_passed = call_time - start;
            EXPECT_LT(
                time_passed,
                (repeat_interval + max_discrepancy) * call_count
            ) << "call_count: " << call_count;
            EXPECT_GT(
                time_passed,
                (repeat_interval - max_discrepancy) * call_count
            ) << "call_count: " << call_count;
        }

        // Starting with the second call, start comparing the delay.
        if( call_count > 1 ){
            auto time_passed = call_time - previous_call;
            EXPECT_LT(
                time_passed,
                repeat_interval + max_discrepancy
            ) << "call_count: " << call_count;
            EXPECT_GT(
                time_passed,
                repeat_interval - max_discrepancy
            ) << "call_count: " << call_count;
        }

        previous_call = call_time;

        // Stop repeating after 4 calls.
        ASSERT_LT( call_count, 5 );
        if( call_count == 4 ){
            repeat_timeout.stop();
        }
    }).then([&](){
        EXPECT_FALSE( resolved );
        resolved = true;
    });
    EXPECT_FALSE( resolved );
    EXPECT_EQ( 0, call_count );

    start = clock::now();
    loop.run();
    EXPECT_EQ( 4, call_count );
    EXPECT_TRUE( resolved );
}

// ---------------------------------------------------------------------------------------------- //

TEST_F( TimeoutTests, Stop ){
    time_point start;
    bool rejected = false;

    event::Timeout timeout( loop );
    timeout.start( short_delay * 5 ).then([&](){
        FAIL() << "Timeout promise was resolved, not rejected.";
    }, [&]( const error::Exception& err ){
        EXPECT_EQ( 1, err.error_code() );
        EXPECT_EQ( (std::string)"Timeout cancelled.", err.what() );

        rejected = true;
    });
    EXPECT_FALSE( rejected );

    event::wait( loop, short_delay ).then([&](){
        timeout.stop();
    });

    start = clock::now();
    loop.run();
    EXPECT_TRUE( rejected );
}

}
}
