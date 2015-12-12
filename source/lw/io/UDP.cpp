
#include <cstdlib>
#include <uv.h>

#include "lw/io/UDP.hpp"

namespace lw {
namespace io {

UDP::UDP(event::Loop& loop):
    BasicStream(_make_state(loop))
{}

// ---------------------------------------------------------------------------------------------- //

uv_stream_s* UDP::_make_state(event::Loop& loop){
    LW_TRACE("Making UDP state.");
    uv_udp_t* udp = (uv_udp_t*)std::malloc(sizeof(uv_udp_t));
    uv_udp_init(loop.lowest_layer(), udp);

    return (uv_stream_s*)udp;
}

}
}
