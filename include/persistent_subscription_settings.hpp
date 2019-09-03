#pragma once

#ifndef ES_PERSISTENT_SUBSCRIPTION_SETTINGS_HPP
#define ES_PERSISTENT_SUBSCRIPTION_SETTINGS_HPP

#include <string>
#include <chrono>

#include "system/consumer_strategies.hpp"

namespace es {

class persistent_subscription_settings_builder;

class persistent_subscription_settings
{
public:
	friend class persistent_subscription_settings_builder;

	persistent_subscription_settings(persistent_subscription_settings const& other) = default;
	persistent_subscription_settings(persistent_subscription_settings&& other) = default;

	bool resolve_link_tos() const { return resolve_link_tos_; }
	std::int64_t start_from() const { return start_from_; }
	bool with_extra_statistics() const { return extra_statistics_; }
	std::chrono::nanoseconds message_timeout() const { return message_timeout_; }
	int max_retry_count() const { return max_retry_count_; }
	int live_buffer_size() const { return live_buffer_size_; }
	int history_buffer_size() const { return history_buffer_size_; }
	int read_batch_size() const { return read_batch_size_; }
	std::chrono::nanoseconds checkpoint_after() const { return checkpoint_after_; }
	int min_checkpoint_count() const { return min_checkpoint_count_; }
	int max_checkpoint_count() const { return max_checkpoint_count_; }
	int max_subscriber_count() const { return max_subscriber_count_; }
	std::string const& named_consumer_strategy() const { return named_consumer_strategy_; }

private:
	persistent_subscription_settings() = default;

	bool resolve_link_tos_ = false;
	std::int64_t start_from_;
	bool extra_statistics_ = false;
	std::chrono::nanoseconds message_timeout_ = std::chrono::seconds(30);
	int max_retry_count_ = 10;
	int live_buffer_size_ = 500;
	int history_buffer_size_ = 500;
	int read_batch_size_ = 20;
	std::chrono::nanoseconds checkpoint_after_ = std::chrono::seconds(2);
	int min_checkpoint_count_ = 10;
	int max_checkpoint_count_ = 1000;
	int max_subscriber_count_ = 0;
	std::string named_consumer_strategy_ = system::consumer_strategies::round_robin;
};

class persistent_subscription_settings_builder
{
public:
	using self_type = persistent_subscription_settings_builder;

	self_type& resolve_link_tos() { settings_.resolve_link_tos_ = true; return *this; }
	self_type& do_not_resolve_link_tos() { settings_.resolve_link_tos_ = false; return *this; }
	self_type& with_extra_statistics() { settings_.extra_statistics_ = true; return *this; }
	self_type& prefer_round_robin() { settings_.named_consumer_strategy_ = system::consumer_strategies::round_robin; return *this; }
	self_type& prefer_dispatch_to_single() { settings_.named_consumer_strategy_ = system::consumer_strategies::dispatch_to_single; return *this; }
	self_type& start_from_beginning() { settings_.start_from_ = 0; return *this; }
	self_type& start_from(std::int64_t position) { settings_.start_from_ = position; return *this; }
	self_type& with_message_timeout(std::chrono::nanoseconds timeout) { settings_.message_timeout_ = timeout; return *this; }
	self_type& dont_timeout_messages() { settings_.message_timeout_ = std::chrono::nanoseconds(0); return *this; }
	self_type& checkpoint_after(std::chrono::nanoseconds time) { settings_.checkpoint_after_ = time; return *this; }
	self_type& minimum_checkpoint_count_of(int count) { settings_.min_checkpoint_count_ = count; return *this; }
	self_type& maximum_checkpoint_count_of(int count) { settings_.max_checkpoint_count_ = count; return *this; }
	self_type& with_max_retries_of(int count) { settings_.max_retry_count_ = count; return *this; }
	self_type& with_live_buffer_size_of(int size) { settings_.live_buffer_size_ = size; return *this; }
	self_type& with_read_batch_size_of(int size) { settings_.read_batch_size_ = size; return *this; }
	self_type& with_history_buffer_size_of(int size) { settings_.history_buffer_size_ = size; return *this; }
	self_type& start_from_current() { settings_.start_from_ = -1; }
	self_type& with_max_subscriber_count_of(int count) { settings_.max_subscriber_count_ = count; return *this; }
	self_type& with_named_consumer_strategy(std::string_view strategy) { settings_.named_consumer_strategy_ = strategy; return *this; }
	persistent_subscription_settings build() { return std::move(settings_); }

private:
	persistent_subscription_settings settings_;
};

}

#endif // ES_PERSISTENT_SUBSCRIPTION_SETTINGS_HPP