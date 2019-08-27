#pragma once

#ifndef ES_AUTH_INFO_HPP
#define ES_AUTH_INFO_HPP

#include <chrono>

#include "guid.hpp"

namespace es {
namespace detail {
namespace connection {

template <class Clock>
class authentication_info
{
public:
	authentication_info() = default;

    explicit authentication_info(
		guid_type correlation_id,
        typename Clock::duration timestamp
    ) 
    :   correlation_id_(correlation_id),
        timestamp_(timestamp)
    {}

    explicit authentication_info(authentication_info const& other) = default;
    authentication_info(authentication_info&& other) noexcept = default;
    authentication_info& operator=(authentication_info const& other) = default;
    authentication_info& operator=(authentication_info&& other) noexcept = default;

	void set_correlation_id(guid_type const& corr_id) { correlation_id_ = corr_id; }
	void set_timestamp(typename Clock::duration const& ts) { timestamp_ = ts; }
    guid_type const& correlation_id() const { return correlation_id_; }
    typename Clock::duration const& timestamp() const { return timestamp_; }

private:
	guid_type correlation_id_;
    typename Clock::duration timestamp_;
};

}
}
}

#endif // ES_AUTH_INFO_HPP