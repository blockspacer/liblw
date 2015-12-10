#pragma once

#define LW_NOOP ((void)0)
#define LW_EXPAND(...) __VA_ARGS__
#define LW_CONCAT(x, y) x##y
#define LW_STRINGIFY_IMPL(x) #x
#define LW_STRINGIFY(x) LW_STRINGIFY_IMPL(x)
#define LW_FILENAME (std::strstr(__FILE__, "liblw") ? std::strstr(__FILE__, "liblw") : __FILE__)
