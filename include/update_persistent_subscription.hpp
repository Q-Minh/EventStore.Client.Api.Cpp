#pragma once

/*
Add user credentials parameter to have authentication per request
*/

#ifndef ES_UPDATE_PERSISTENT_SUBSCRIPTION_HPP
#define ES_UPDATE_PERSISTENT_SUBSCRIPTION_HPP

#include <string>
#include <optional>

#include "message/messages.pb.h"

#include "duration_conversions.hpp"
#include "guid.hpp"
#include "persistent_subscription_settings.hpp"
#include "tcp/handle_operation_error.hpp"

namespace es {

template <class ConnectionType, class UpdatePersistentSubscriptionResultHandler>
void async_update_persistent_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	std::string_view stream,
	std::string_view group,
	persistent_subscription_settings const& settings,
	UpdatePersistentSubscriptionResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<UpdatePersistentSubscriptionResultHandler, boost::system::error_code, std::string>,
		"UpdatePersistentSubscriptionResultHandler requirements not met, must have signature R(boost::system::error_code, std::string)"
		);

	message::UpdatePersistentSubscription request;
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
			detail::tcp::tcp_command::update_persistent_subscription,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::update_persistent_subscription,
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
		[&ioc = connection->get_io_context(), handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec && view.command() != detail::tcp::tcp_command::update_persistent_subscription_completed)
		{
			//ec = make_error_code(communication_errors::unexpected_response);
			auto& discovery_service = boost::asio::use_service<typename ConnectionType::discovery_service_type>(ioc);
			detail::handle_operation_error(ec, view, discovery_service);
		}

		// if there was an error, report immediately
		if (ec)
		{
			handler(ec, {});
			return;
		}

		message::UpdatePersistentSubscriptionCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::UpdatePersistentSubscriptionCompleted_UpdatePersistentSubscriptionResult_Success:
			break;
		case message::UpdatePersistentSubscriptionCompleted_UpdatePersistentSubscriptionResult_Fail:
			ec = make_error_code(subscription_errors::persistent_subscription_fail);
			break;
		case message::UpdatePersistentSubscriptionCompleted_UpdatePersistentSubscriptionResult_AccessDenied:
			ec = make_error_code(subscription_errors::access_denied);
			break;
		case message::UpdatePersistentSubscriptionCompleted_UpdatePersistentSubscriptionResult_DoesNotExist:
			ec = make_error_code(subscription_errors::persistent_subscription_does_not_exist);
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

#endif // ES_UPDATE_PERSISTENT_SUBSCRIPTION_HPP