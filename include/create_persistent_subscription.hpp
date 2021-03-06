#pragma once

/*
Add user credentials parameter to have authentication per request
*/

#ifndef ES_CREATE_PERSISTENT_SUBSCRIPTION_HPP
#define ES_CREATE_PERSISTENT_SUBSCRIPTION_HPP

#include <string>
#include <optional>

#include "message/messages.pb.h"

#include "duration_conversions.hpp"
#include "guid.hpp"
#include "persistent_subscription_settings.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class CreatePersistentSubscriptionResultHandler>
void async_create_persistent_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	std::string_view stream,
	std::string_view group,
	persistent_subscription_settings const& settings,
	CreatePersistentSubscriptionResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<CreatePersistentSubscriptionResultHandler, boost::system::error_code, std::string>,
		"CreatePersistentSubscriptionResultHandler requirements not met, must have signature R(boost::system::error_code, std::string)"
	);

	message::CreatePersistentSubscription request;
	request.set_event_stream_id(std::string(stream));
	request.set_buffer_size(settings.history_buffer_size());
	request.set_checkpoint_after_time(static_cast<std::int32_t>(ES_MILLISECONDS(settings.checkpoint_after())));
	request.set_checkpoint_max_count(settings.max_checkpoint_count());
	request.set_checkpoint_min_count(settings.min_checkpoint_count());
	request.set_live_buffer_size(settings.live_buffer_size());
	request.set_max_retry_count(settings.max_retry_count());
	request.set_message_timeout_milliseconds(static_cast<std::int32_t>(ES_MILLISECONDS(settings.message_timeout())));
	request.set_named_consumer_strategy(settings.named_consumer_strategy());
	request.set_prefer_round_robin(settings.named_consumer_strategy() == system::consumer_strategies::round_robin);
	request.set_read_batch_size(settings.read_batch_size());
	request.set_record_statistics(settings.with_extra_statistics());
	request.set_resolve_link_tos(settings.resolve_link_tos());
	request.set_start_from(settings.start_from());
	request.set_subscriber_max_count(settings.max_subscriber_count());
	request.set_subscription_group_name(std::string(group));

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();
	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::create_persistent_subscription,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::create_persistent_subscription,
			detail::tcp::tcp_flags::authenticated,
			corr_id,
			connection->settings().default_user_credentials().username(),
			connection->settings().default_user_credentials().password(),
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}

	connection->async_send(
		std::move(package),
		[handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec && view.command() != detail::tcp::tcp_command::create_persistent_subscription_completed)
		{
			ec = make_error_code(communication_errors::unexpected_response);
		}

		// if there was an error, report immediately
		if (ec)
		{
			handler(ec, {});
			return;
		}

		message::CreatePersistentSubscriptionCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::CreatePersistentSubscriptionCompleted_CreatePersistentSubscriptionResult_Success:
			break;
		case message::CreatePersistentSubscriptionCompleted_CreatePersistentSubscriptionResult_Fail:
			ec = make_error_code(subscription_errors::persistent_subscription_fail);
			break;
		case message::CreatePersistentSubscriptionCompleted_CreatePersistentSubscriptionResult_AccessDenied:
			ec = make_error_code(subscription_errors::access_denied);
			break;
		case message::CreatePersistentSubscriptionCompleted_CreatePersistentSubscriptionResult_AlreadyExists:
			ec = make_error_code(subscription_errors::persistent_subscription_already_exists);
			break;
		default:
			ec = make_error_code(communication_errors::unexpected_response);
			break;
		}

		handler(ec, response.has_reason() ? response.reason() : "");
		return;
	}
	);
}

} // es

#endif // ES_CREATE_PERSISTENT_SUBSCRIPTION_HPP