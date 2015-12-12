
#include <cstdlib>
#include <uv.h>

#include "lw/io/TCP.hpp"

namespace lw {
namespace io {

TCP::TCP(event::Loop& loop):
    BasicStream(_make_state(loop))
{}

// ---------------------------------------------------------------------------------------------- //

uv_stream_s* TCP::_make_state(event::Loop& loop){
    LW_TRACE("Making TCP state.");
    uv_tcp_t* tcp = (uv_tcp_t*)std::malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop.lowest_layer(), tcp);

    return (uv_stream_s*)tcp;
}

}
}
