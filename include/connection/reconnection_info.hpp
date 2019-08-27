#pragma once

#ifndef ES_RECONNECTION_INFO_HPP
#define ES_RECONNECTION_INFO_HPP

#include <chrono>

namespace es {
namespace detail {
namespace connection {

template <class Clock>
class reconnection_info
{
public:
	reconnection_info() = default;

    explicit reconnection_info(
        int reconnection_attempt_no,
        typename Clock::duration timestamp
    ) 
    :   reconnection_attempt_no_(reconnection_attempt_no),
        timestamp_(timestamp)
    {}

    explicit reconnection_info(reconnection_info const& other) = default;
    reconnection_info(reconnection_info&& other) noexcept = default;
    reconnection_info& operator=(reconnection_info const& other) = default;
    reconnection_info& operator=(reconnection_info&& other) noexcept = default;

	void set_reconnection_attempt_no(int no) { reconnection_attempt_no_ = no; }
	void set_timestamp(typename Clock::duration const& timestamp) { timestamp_ = timestamp; }
    int reconnection_attempt_no() const { return reconnection_attempt_no_; }
    typename Clock::duration const& timestamp() const { return timestamp_; }

private:
    int reconnection_attempt_no_;
    typename Clock::duration timestamp_;
};

} // connection
} // detail
} // es

#endif // ES_RECONNECTION_INFO_HPP