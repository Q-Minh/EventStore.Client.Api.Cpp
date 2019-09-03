#pragma once

#ifndef ES_CATCHUP_SUBSCRIPTION_SETTINGS_HPP
#define ES_CATCHUP_SUBSCRIPTION_SETTINGS_HPP

#include <string_view>

namespace es {

class catchup_subscription_settings_builder;

class catchup_subscription_settings
{
public:
	friend class catchup_subscription_settings_builder;

	catchup_subscription_settings(catchup_subscription_settings const& other) = default;
	catchup_subscription_settings(catchup_subscription_settings&& other) = default;

	int max_live_queue_size() const { return max_live_queue_size_; }
	int read_batch_size() const { return read_batch_size_; }
	bool resolve_link_tos() const { return resolve_link_tos_; }
	std::string const& subscription_name() const { return subscription_name_; }

private:
	catchup_subscription_settings() = default;

	int max_live_queue_size_ = 500;
	int read_batch_size_ = 10;
	bool resolve_link_tos_ = true;
	std::string subscription_name_;
};

class catchup_subscription_settings_builder
{
public:
	using self_type = catchup_subscription_settings_builder;

	self_type& with_max_live_queue_size(int size) { settings_.max_live_queue_size_ = size; return *this; }
	self_type& with_read_batch_size(int size) { settings_.read_batch_size_ = size; return *this; }
	self_type& resolve_link_tos(bool resolve) { settings_.resolve_link_tos_ = resolve; return *this; }
	self_type& with_subscription_name(std::string_view name) { settings_.subscription_name_ = name; return *this; }
	catchup_subscription_settings build() { return std::move(settings_); }

private:
	catchup_subscription_settings settings_;
};

}

#endif // ES_CATCHUP_SUBSCRIPTION_SETTINGS_HPP
