#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/address.hpp>

#include "read_all_events_forward.hpp"
#include "read_all_events_backward.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 10)
	{
		ES_ERROR("expected 9 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <from-commit-pos> <from-prepare-pos> <max-number-of-events> [forward | backward] [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./read-all-events 127.0.0.1 1113 admin changeit 0 0 10  forward info");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::int64_t commit_position = std::stoll(argv[5]);
	std::int64_t prepare_position = std::stoll(argv[6]);
	int max_count = std::stoi(argv[7]);

	std::string direction_str = argv[8];
	if (direction_str != "forward" && direction_str != "backward")
	{
		ES_ERROR("read direction can only be backward or forward");
		return 0;
	}

	std::string_view lvl = argv[9];
	ES_DEFAULT_LOG_LEVEL(lvl);

	asio::io_context ioc;

	asio::ip::tcp::endpoint endpoint;
	endpoint.address(asio::ip::make_address_v4(ep));
	endpoint.port(port);

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::discovery_service;
	auto& discovery_service = asio::make_service<discovery_service_type>(ioc, endpoint, asio::ip::tcp::endpoint(), false);

	// parameterize our tcp connection with steady timer, the basic discovery service and our type-erased operation
	using connection_type =
		es::connection::basic_tcp_connection<
		asio::steady_timer,
		discovery_service_type,
		es::operation<>
		>;

	// hold storage for the tcp connection to read into
	// this is not necessary, we only need to do this because asio's dynamic buffer
	// does not own the storage, but any type satisfying DynamicBuffer requirements
	// will do
	std::vector<std::uint8_t> buffer_storage;

	std::shared_ptr<connection_type> tcp_connection =
		std::make_shared<connection_type>(
			ioc,
			connection_settings,
			asio::dynamic_buffer(buffer_storage)
			);

	// wait for connection before sending operations
	bool is_connected{ false };
	bool notification{ false };

	// the async connect will call the given completion handler
	// on error or success, we can inspect the error_code for more info
	tcp_connection->async_connect(
		[tcp_connection = tcp_connection, &is_connected, &notification](asio::error_code ec, es::detail::tcp::tcp_package_view view)
	{
		notification = true;

		if (!ec || ec == es::connection_errors::authentication_failed)
		{
			// on success, command should be client identified
			if (view.command() != es::detail::tcp::tcp_command::client_identified) return;

			ES_INFO("client has been identified, {}", ec.message());
			is_connected = true;
		}
		else
		{
			// es connection failed
			ES_ERROR("client has failed to identify with server, {}", ec.message());
		}
	}
	);

	// run non-blocking loop until notification notifies us of the connection operation result
	while (!notification)
	{
		ioc.poll();
	}

	if (!is_connected)
	{
		ES_ERROR("failed to connect to EventStore");
		return 0;
	}

	auto handler = [tcp_connection = tcp_connection, &direction_str](std::error_code ec, std::optional<es::all_events_slice> result)
	{
		if (!ec)
		{
			auto value = result.value(); 
			ES_INFO("from-commit-position={}, from-prepare-position={}, next-commit-position={}, next-prepare-position={}, is-end-of-stream={}, read-direction={}",
				value.from_position().commit_position(),
				value.from_position().prepare_position(),
				value.next_position().commit_position(),
				value.next_position().prepare_position(),
				value.is_end_of_stream(),
				value.stream_read_direction() == es::read_direction::forward ? "forward" : "backward"
			);

			for (auto const& event : value.events())
			{
				ES_TRACE("event read\n\tis-resolved={}\n\tevent-id={}\n\tevent-no={}\n\tevent-type={}\n\tmetadata={}\n\tcontent={}",
					event.is_resolved(),
					es::to_string(event.event().value().event_id()),
					event.event().value().event_number(),
					event.event().value().event_type(),
					event.event().value().metadata(),
					event.event().value().content()
				);
			}

			ES_INFO("got {} events", value.events().size());
		}
		else
		{
			ES_ERROR("error trying to read all events {} : {}", direction_str, ec.message());
			return;
		}
	};

	es::position from_position{ commit_position, prepare_position };

	// read stream events in one direction or the other
	if (direction_str == "forward")
	{
		es::async_read_all_events_forward(
			tcp_connection,
			from_position,
			max_count,
			true,
			std::move(handler)
		);
	}
	else
	{
		es::async_read_all_events_backward(
			tcp_connection,
			from_position,
			max_count,
			true,
			std::move(handler)
		);
	}

	ioc.run();

	return 0;
}