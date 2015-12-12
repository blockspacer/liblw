#pragma once

#include "lw/event.hpp"

namespace lw {
namespace io {

class UDP : public event::BasicStream {
public:
    UDP(event::Loop& loop);

private:
    static uv_stream_s* _make_state(event::Loop& loop);
};

}
}
