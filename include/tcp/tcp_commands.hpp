#pragma once

#ifndef TCP_COMMANDS_HPP
#define TCP_COMMANDS_HPP

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
    data_chunk_culk = 0x14,
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

} // tcp
} // detail
} // es

#endif // TCP_COMmANDS_HPP