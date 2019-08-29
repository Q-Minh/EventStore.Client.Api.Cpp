#pragma once

#ifndef ES_TRANSACTIONAL_WRITE_HPP
#define ES_TRANSACTIONAL_WRITE_HPP

#include "message/messages.pb.h"

#include "guid.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class NewEventsSequence, class TransactionalWriteHandler>
void async_transactional_write(
	std::shared_ptr<ConnectionType> const& connection,
	std::int64_t transaction_id,
	NewEventsSequence&& events,
	TransactionalWriteHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<TransactionalWriteHandler, std::error_code>,
		"TransactionalWriteHandler requirements not met, must have signature R(std::error_code)"
	);

	message::TransactionWrite request;
	request.set_transaction_id(transaction_id);
	request.set_require_master(connection->settings().require_master());

	auto it = std::begin(events);
	auto end = std::end(events);

	for (; it != end; ++it)
	{
		auto* new_event = request.add_events();
		if constexpr (std::is_same_v<es::guid_type, std::decay_t<decltype(it->event_id())>>)
		{
			new_event->set_event_id(it->event_id().data, it->event_id().size());
		}
		else
		{
			new_event->set_event_id(std::move(it->event_id()));
		}

		// we just try to move to prevent copying
		new_event->set_event_type(std::move(it->type()));
		new_event->set_data_content_type(it->is_json() ? 1 : 0);
		new_event->set_metadata_content_type(0); // see .NET client api
		new_event->set_data(std::move(it->content()));
		new_event->set_metadata(std::move(it->metadata()));
	}

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_write,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_write,
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
		[handler = std::move(handler), connection = connection]
		(std::error_code ec, detail::tcp::tcp_package_view view)
		{
			if (!ec && view.command() != detail::tcp::tcp_command::transaction_write_completed)
			{
				ec = make_error_code(communication_errors::unexpected_response);
			}

			if (ec)
			{
				handler(ec);
				return;
			}

			message::TransactionWriteCompleted response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			switch (response.result())
			{
			case message::OperationResult::Success:
				break;
			case message::OperationResult::PrepareTimeout:
				// retry
				break;
			case message::OperationResult::CommitTimeout:
				// retry
				break;
			case message::OperationResult::ForwardTimeout:
				// retry
				break;
			case message::OperationResult::AccessDenied:
				ec = make_error_code(stream_errors::access_denied);
				break;
			default:
				ec = make_error_code(communication_errors::unexpected_response);
				break;
			}

			handler(ec);
		}
	);
}

}

#endif // ES_TRANSACTIONAL_WRITE_HPP