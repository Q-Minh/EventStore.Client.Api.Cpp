#pragma once

#ifndef ES_TCP_COMMANDS_HPP
#define ES_TCP_COMMANDS_HPP

#include <cstdint>

namespace es {
namespace detail {
namespace tcp {

enum class tcp_command : std::uint8_t
{
    heartbeat_request_command = 0x01,
    heartbeat_response_command = 0x02,

    ping = 0x03,
    pong = 0x04,

    prepare_ack = 0x05,
    commit_ack = 0x06,

    slave_assignment = 0x07,
    clone_assignment = 0x08,

    subscribe_replica = 0x10,
    replica_log_position_ack = 0x11,
    create_chunk = 0x12,
    raw_chunk_bulk = 0x13,
    data_chunk_bulk = 0x14,
    replica_subscription_retry = 0x15,
    replica_subscribed = 0x16,

    // CLIENT COMMANDS
    // create_stream = 0x80,
    // create_stream_completed = 0x81,

    write_events = 0x82,
    write_events_completed = 0x83,

    transaction_start = 0x84,
    transaction_start_completed = 0x85,
    transaction_write = 0x86,
    transaction_write_completed = 0x87,
    transaction_commit = 0x88,
    transaction_commit_completed = 0x89,

    delete_stream = 0x8A,
    delete_stream_completed = 0x8B,

    read_event = 0xB0,
    read_event_completed = 0xB1,
    read_stream_events_forward = 0xB2,
    read_stream_events_forward_completed = 0xB3,
    read_stream_events_backward = 0xB4,
    read_stream_events_backward_completed = 0xB5,
    read_all_events_forward = 0xB6,
    read_all_events_forward_completed = 0xB7,
    read_all_events_backward = 0xB8,
    read_all_events_backward_completed = 0xB9,

    subscribe_to_stream = 0xC0,
    subscription_confirmation = 0xC1,
    stream_event_appeared = 0xC2,
    unsubscribe_from_stream = 0xC3,
    subscription_dropped = 0xC4,
    connect_to_persistent_subscription = 0xC5,
    persistent_subscription_confirmation = 0xC6,
    persistent_subscription_stream_event_appeared = 0xC7,
    create_persistent_subscription = 0xC8,
    create_persistent_subscription_completed = 0xC9,
    delete_persistent_subscription = 0xCA,
    delete_persistent_subscription_completed = 0xCB,
    persistent_subscription_ack_events = 0xCC,
    persistent_subscription_nak_events = 0xCD,
    update_persistent_subscription = 0xCE,
    update_persistent_subscription_completed = 0xCF,

    scavenge_database = 0xD0,
    scavenge_database_completed = 0xD1,

    bad_request = 0xF0,
    not_handled = 0xF1,
    authenticate = 0xF2,
    authenticated = 0xF3,
    not_authenticated = 0xF4,
    identify_client = 0xF5,
    client_identified = 0xF6,
};

inline const char* to_string(tcp_command cmd)
{
	switch (cmd)
	{
	case tcp_command::heartbeat_request_command:
		return "heartbeat request";
	case tcp_command::heartbeat_response_command:
		return "heartbeat response";
	case tcp_command::ping:
		return "ping";
	case tcp_command::pong:
		return "pong";
	case tcp_command::prepare_ack:
		return "prepare ack";
	case tcp_command::commit_ack:
		return "commit ack";
	case tcp_command::slave_assignment:
		return "slave assignment";
	case tcp_command::clone_assignment:
		return "clone assignment";
	case tcp_command::subscribe_replica:
		return "subscribe replica";
	case tcp_command::replica_log_position_ack:
		return "replica log position ack";
	case tcp_command::create_chunk:
		return "create chunk";
	case tcp_command::raw_chunk_bulk:
		return "raw chunk bulk";
	case tcp_command::data_chunk_bulk:
		return "data chunk bulk";
	case tcp_command::replica_subscription_retry:
		return "replica subscription retry";
	case tcp_command::replica_subscribed:
		return "replica subscribed";
	case tcp_command::write_events:
		return "write events";
	case tcp_command::write_events_completed:
		return "write events completed";
	case tcp_command::transaction_start:
		return "transaction start";
	case tcp_command::transaction_start_completed:
		return "transaction start completed";
	case tcp_command::transaction_write:
		return "transaction write";
	case tcp_command::transaction_write_completed:
		return "transaction write completed";
	case tcp_command::transaction_commit:
		return "transaction commit";
	case tcp_command::transaction_commit_completed:
		return "transaction commit completed";
	case tcp_command::delete_stream:
		return "delete stream";
	case tcp_command::delete_stream_completed:
		return "delete stream completed";
	case tcp_command::read_event:
		return "read event";
	case tcp_command::read_event_completed:
		return "read event completed";
	case tcp_command::read_stream_events_forward:
		return "read stream events forward";
	case tcp_command::read_stream_events_forward_completed:
		return "read stream events forward completed";
	case tcp_command::read_stream_events_backward:
		return "read stream events backward";
	case tcp_command::read_stream_events_backward_completed:
		return "read stream events backward completed";
	case tcp_command::read_all_events_forward:
		return "read all events forward";
	case tcp_command::read_all_events_forward_completed:
		return "read all events forward completed";
	case tcp_command::read_all_events_backward:
		return "read all events backward";
	case tcp_command::read_all_events_backward_completed:
		return "read all events backward completed";
	case tcp_command::subscribe_to_stream:
		return "subscribe to stream";
	case tcp_command::subscription_confirmation:
		return "subscription confirmation";
	case tcp_command::stream_event_appeared:
		return "stream event appeared";
	case tcp_command::unsubscribe_from_stream:
		return "unsubscribe from stream";
	case tcp_command::subscription_dropped:
		return "subscription dropped";
	case tcp_command::connect_to_persistent_subscription:
		return "connect to persistent subscription";
	case tcp_command::persistent_subscription_confirmation:
		return "persistent subscription confirmation";
	case tcp_command::persistent_subscription_stream_event_appeared:
		return "persistent subscription stream event appeared";
	case tcp_command::create_persistent_subscription:
		return "create persistent subscription";
	case tcp_command::create_persistent_subscription_completed:
		return "create persistent subscription completed";
	case tcp_command::delete_persistent_subscription:
		return "delete persistent subscription";
	case tcp_command::delete_persistent_subscription_completed:
		return "delete persistent subscription completed";
	case tcp_command::persistent_subscription_ack_events:
		return "persistent subscription ack events";
	case tcp_command::persistent_subscription_nak_events:
		return "persistent subscription nak events";
	case tcp_command::update_persistent_subscription:
		return "update persistent subscription";
	case tcp_command::update_persistent_subscription_completed:
		return "update persistent subscription completed";
	case tcp_command::scavenge_database:
		return "scavenge database";
	case tcp_command::scavenge_database_completed:
		return "scavenge database completed";
	case tcp_command::bad_request:
		return "bad request";
	case tcp_command::not_handled:
		return "not handled";
	case tcp_command::authenticate:
		return "authenticate";
	case tcp_command::authenticated:
		return "authenticated";
	case tcp_command::not_authenticated:
		return "not authenticated";
	case tcp_command::identify_client:
		return "identify client";
	case tcp_command::client_identified:
		return "client identified";
	default:
		return "unknown";
	}
}

} // tcp
} // detail
} // es

#endif // TCP_COMMANDS_HPP