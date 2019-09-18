#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include <subscription/volatile_subscription.hpp>
#include <connection/connection.hpp>

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

	auto& discovery_service = boost::asio::make_service<es::single_node_discovery_service>(ioc, endpoint, boost::asio::ip::tcp::endpoint(), false);
	auto tcp_connection = std::make_shared<es::single_node_tcp_connection>(ioc, connection_settings);

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

	auto guid = es::guid();
	if (stream == "all")
	{
		auto subscription = es::make_volatile_all_subscription(tcp_connection, guid);
		ES_INFO("creating volatile subscription to stream={}, subscription-id={}", stream, es::to_string(guid));
		subscription->async_start(
			[self = subscription](es::resolved_event const& event)
		{
			ES_INFO("event received, {}", es::to_string(event.event().value().event_id()));
		},
			[self = subscription](boost::system::error_code ec, es::subscription::volatile_subscription<es::single_node_tcp_connection> const& subscription)
		{
			ES_ERROR("subscription dropped : {}", ec.message());
		}
		);
	}
	else
	{
		auto subscription = es::make_volatile_subscription(tcp_connection, guid, stream);
		ES_INFO("creating volatile subscription to stream={}, subscription-id={}", stream, es::to_string(guid));
		subscription->async_start(
			[self = subscription](es::resolved_event const& event)
		{
			ES_INFO("event received, {}", es::to_string(event.event().value().event_id()));
		},
			[self = subscription](boost::system::error_code ec, es::subscription::volatile_subscription<es::single_node_tcp_connection> const& subscription)
		{
			ES_ERROR("subscription dropped : {}", ec.message());
		}
		);
	}

	ioc.run();

	return 0;
}