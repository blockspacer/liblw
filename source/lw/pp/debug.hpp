#pragma once

#include "lw/pp/util.hpp"

#ifdef LW_ENABLE_TRACE
#   include <cstring>
#   include <iostream>
#   define LW_TRACE(m) std::cout << LW_FILENAME << "[" << __LINE__ "] " << m << std::endl
#else
#   define LW_TRACE(m) LW_NOOP
#endif

#if defined(LW_DEBUG) || !defined(NDEBUG)
#   ifndef LW_DEBUG
#       define LW_DEBUG
#   endif
#   ifndef DEBUG
#       define DEBUG
#   endif
#   ifdef NDEBUG
#       undef NDEBUG
#   endif
#else
#   ifndef NDEBUG
#       define NDEBUG
#   endif
#endif

#ifdef LW_DEBUG
#   include <cassert>
#   define LW_ASSERT(x) assert(x)
#else
#   define LW_ASSERT(x) LW_NOOP
#endif
