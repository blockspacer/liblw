#pragma once

#include "lw/event/Promise.hpp"
#include "lw/event/Promise.void.hpp"

namespace lw {
namespace event {

template< typename T >
inline Future< T > Promise< T >::future( void ){
    return Future< T >( m_state );
}

// -------------------------------------------------------------------------- //

inline Future< void > Promise< void >::future( void ){
    return Future< void >( m_state );
}

}
}