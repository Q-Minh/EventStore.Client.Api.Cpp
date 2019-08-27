#pragma once

#ifndef ES_IDENTITY_INFO_HPP
#define ES_IDENTITY_INFO_HPP

#include <chrono>

#include "guid.hpp"

namespace es {
namespace detail {
namespace connection {

template <class Clock>
class identity_info
{
public:
	identity_info() = default;

    explicit identity_info(
		guid_type const& correlation_id,
        typename Clock::duration timestamp
    ) 
    :   correlation_id_(correlation_id),
        timestamp_(timestamp)
    {}

    explicit identity_info(identity_info const& other) = default;
    identity_info(identity_info&& other) noexcept = default;
    identity_info& operator=(identity_info const& other) = default;
    identity_info& operator=(identity_info&& other) noexcept = default;

	void set_correlation_id(guid_type const& correlation_id) { correlation_id_ = correlation_id; }
	void set_timestamp(typename Clock::duration timestamp) { timestamp_ = timestamp; }
    guid_type const& correlation_id() const { return correlation_id_; }
    typename Clock::duration const& timestamp() const { return timestamp_; }

private:
	guid_type correlation_id_;
    typename Clock::duration timestamp_;
};

} // connection
} // detail
} // es

#endif // ES_IDENTITY_INFO_HPP