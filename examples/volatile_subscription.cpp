#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/address.hpp>

#include "subscription/volatile_subscription.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 7)
	{
		ES_ERROR("expected 6 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <stream-name> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./volatile-subscription 127.0.0.1 1113 admin changeit test-stream info");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string_view stream = argv[5];
	std::string_view lvl = argv[6];
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
		[tcp_connection = tcp_connection, &is_connected, &notification](asio::error_code ec, std::optional<es::connection_result> result)
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

	auto guid = es::guid();
	auto subscription = std::make_shared<es::subscription::volatile_subscription<connection_type>>(tcp_connection, guid, stream);
	ES_INFO("creating volatile subscription to stream={}, subscription-id={}", stream, es::to_string(guid));
	subscription->async_start(
		[self = subscription](es::resolved_event& event)
	{
		ES_INFO("event received, {}", es::to_string(event.event().value().event_id()));
	},
		[](std::error_code ec, es::subscription::volatile_subscription<connection_type> const& subscription)
	{
		ES_ERROR("subscription dropped : {}", ec.message());
	}
	);

	ioc.run();

	return 0;
}