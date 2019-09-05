#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "append_to_stream.hpp"
#include "event_data.hpp"

#include "subscription/catchup_all_subscription.hpp"
#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 9)
	{
		ES_ERROR("expected 8 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <from-prepare-position> <from-commit-position> <read-batch-size> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./catchup-all-subscription 127.0.0.1 1113 admin changeit 0 0 500 info");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::int64_t from_prepare_position = std::stoll(argv[5]);
	std::int64_t from_commit_position = std::stoll(argv[6]);
	int read_batch_size = std::stoi(argv[7]);
	std::string_view lvl = argv[8];
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

	// hold storage for the tcp connection to read into
	// this is not necessary, we only need to do this because asio's dynamic buffer
	// does not own the storage, but any type satisfying DynamicBuffer requirements
	// will do
	std::vector<std::uint8_t> buffer_storage;

	std::shared_ptr<connection_type> tcp_connection =
		std::make_shared<connection_type>(
			ioc,
			connection_settings,
			boost::asio::dynamic_buffer(buffer_storage)
			);

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
	auto sub_settings = es::catchup_subscription_settings_builder().
		resolve_link_tos(true).
		with_read_batch_size(read_batch_size).
		with_subscription_name("my-all-subscription").
		build();

	auto subscription = es::make_catchup_all_subscription(tcp_connection, guid, es::position(from_commit_position, from_prepare_position), sub_settings);

	bool test = true;
	auto append_test = 
		[&]()
	{
		// just test once, or else it will be an infinite loop 
		// (event received, send another event, so there is another 
		// event received, so we send another event, etc...)
		if (!test) 
			return;
		else 
			test = false;

		std::vector<es::event_data> events;
		es::event_data event{ es::guid(), "Test.Type", true, "{ \"test\": \"data\"}", "test metadata" };
		events.push_back(event);
		std::string stream_name = "test-stream";

		// append one stream event, the given completion handler will
		// be called once a server response with respect to this 
		// operation has been received or has timed out
		es::async_append_to_stream(
			tcp_connection,
			stream_name,
			std::move(events),
			[tcp_connection = tcp_connection, stream_name](boost::system::error_code ec, std::optional<es::write_result> result)
		{
			if (!ec)
			{
				ES_INFO("successfully appended to stream {}, next expected version={}", stream_name, result.value().next_expected_version());
			}
			else
			{
				ES_ERROR("error appending to stream : {}", ec.message());
				return;
			}
		}
		);
	};

	ES_INFO("creating catchup subscription to stream={}, subscription-id={}, starting from commit-position={}, prepare-position={}", "<all>", es::to_string(guid), from_commit_position, from_prepare_position);
	subscription->async_start(
		[self = subscription, &append_test](es::resolved_event const& event)
	{
		ES_INFO("is-resolved={}, original-event-no={}, original-stream-id={}", event.is_resolved(), event.original_event_number(), event.original_stream_id());

		// let's try to test the append during the catching up to see if it messes up our catchup all subscription
		append_test();

		if (event.event().has_value())
		{
			ES_INFO("event received, event-id={}, event-no={}", es::to_string(event.event().value().event_id()), event.event().value().event_number());
		}
	},
		[self = subscription](boost::system::error_code ec, es::subscription::catchup_all_subscription<connection_type> const& subscription)
	{
		ES_ERROR("subscription dropped : {}", ec.message());
	}
	);

	//ioc.run_for(std::chrono::seconds(5));

	//subscription->unsubscribe();

	ioc.run();

	return 0;
}