#pragma once

#include "lw/event.hpp"

namespace lw {
namespace io {

class TCP : public event::BasicStream {
public:
    TCP(event::Loop& loop);

private:
    static uv_stream_s* _make_state(event::Loop& loop);
};

}
}
