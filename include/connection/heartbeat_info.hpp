#pragma once

#ifndef ES_HEARTBEAT_INFO_HPP
#define ES_HEARTBEAT_INFO_HPP

#include <chrono>

namespace es {
namespace detail {
namespace connection {

template <class Clock>
class heartbeat_info
{
public:
	heartbeat_info() = default;

    explicit heartbeat_info(
        int last_package_no,
        bool is_interval_stage,
        typename Clock::duration timestamp
    ) 
    :   last_package_no_(last_package_no),
        timestamp_(timestamp),
        is_interval_stage_(is_interval_stage)
    {}

    explicit heartbeat_info(heartbeat_info const& other) = default;
    heartbeat_info(heartbeat_info&& other) noexcept = default;
    heartbeat_info& operator=(heartbeat_info const& other) = default;
    heartbeat_info& operator=(heartbeat_info&& other) noexcept = default;

	void set_last_package_no(int no) { last_package_no_ = no; }
	void set_is_interval_stage(bool stage) { is_interval_stage_ = stage; }
	void set_timestamp(typename Clock::duration timestamp) { timestamp_ = timestamp; }
    int last_package_no() const { return last_package_no_; }
    bool is_interval_stage() const { return is_interval_stage_; }
	typename Clock::duration const& timestamp() const { return timestamp_; }

private:
    int last_package_no_;
	typename Clock::duration timestamp_;
    bool is_interval_stage_;
};

} // connection
} // detail
} // es

#endif // ES_HEARTBEAT_INFO_HPP