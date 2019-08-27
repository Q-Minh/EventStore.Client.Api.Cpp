#pragma once

#ifndef TCP_FLAGS_HPP
#define TCP_FLAGS_HPP

#include <cstdint>

namespace es {
namespace detail {
namespace tcp {

enum class tcp_flags : std::uint8_t
{
    none = 0x00,
    authenticated = 0x01
};

} // tcp
} // detail
} // es

#endif // TCP_FLAGS_HPP