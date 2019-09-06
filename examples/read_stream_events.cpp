#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "read_stream_events_forward.hpp"
#include "read_stream_events_backward.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 10)
	{
		ES_ERROR("expected 9 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <stream-name> <from-event-number> <max-number-of-events> [forward | backward] [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./read-stream-events 127.0.0.1 1113 admin changeit test-stream 0 10 forward info");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string stream = argv[5];
	std::int64_t from_event_number = std::stoll(argv[6]);
	int max_count = std::stoi(argv[7]);

	std::string direction_str = argv[8];
	if (direction_str != "forward" && direction_str != "backward")
	{
		ES_ERROR("read direction can only be backward or forward");
		return 0;
	}

	std::string_view lvl = argv[9];
	ES_DEFAULT_LOG_LEVEL(lvl);

	boost::asio::io_context ioc;

	boost::asio::ip::tcp::endpoint endpoint;
	endpoint.address(boost::asio::ip::make_address_v4(ep));
	endpoint.port(port);

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.require_master(false)
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::discovery_service;
	auto& discovery_service = boost::asio::make_service<discovery_service_type>(ioc, endpoint, boost::asio::ip::tcp::endpoint(), false);

	// parameterize our tcp connection with steady timer, the basic discovery service and our type-erased operation
	using connection_type =
		es::connection::basic_tcp_connection<
		boost::asio::steady_timer,
		discovery_service_type,
		es::operation<>
		>;

	auto tcp_connection = std::make_shared<connection_type>(ioc, connection_settings);

	// wait for connection before sending operations
	bool is_connected{ false };
	bool notification{ false };

	// the async connect will call the given completion handler
	// on error or success, we can inspect the error_code for more info
	tcp_connection->async_connect(
		[tcp_connection = tcp_connection, &is_connected, &notification](boost::system::error_code ec, std::optional<es::connection_result> result)
	{
		notification = true;

		if (!ec || ec == es::connection_errors::authentication_failed)
		{
			ES_INFO("client has been identified with connection-id={}, {}",
				es::to_string(result.value().connection_id()),
				ec.message()
			);
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

	auto begin = std::chrono::high_resolution_clock::now();

	auto handler = [tcp_connection = tcp_connection, &stream, begin = begin](boost::system::error_code ec, std::optional<es::stream_events_slice> result)
	{
		if (!ec)
		{
			auto end = std::chrono::high_resolution_clock::now();

			auto& value = result.value();
			ES_INFO("stream={}, from-event-number={}, next-event-number={}, last-event-number={}, is-end-of-stream={}, read-direction={}",
				value.stream(),
				value.from_event_number(),
				value.next_event_number(),
				value.last_event_number(),
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

			ES_INFO("got {} events in {} ms", value.events().size(), ES_MILLISECONDS(end - begin));
			return;
		}
		else
		{
			ES_ERROR("error trying to read stream events from stream {} : {}", stream, ec.message());
			return;
		}
	};

	// read stream events in one direction or the other
	if (direction_str == "forward")
	{
		es::async_read_stream_events_forward(
			tcp_connection,
			stream,
			from_event_number,
			max_count,
			true,
			std::move(handler)
		);
	}
	else
	{
		es::async_read_stream_events_backward(
			tcp_connection,
			stream,
			from_event_number,
			max_count,
			true,
			std::move(handler)
		);
	}

	ioc.run();

	return 0;
}