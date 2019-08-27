#pragma once

#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <cstdint>
#include <chrono>

namespace es {
namespace connection {
namespace constants {

inline const std::uint32_t kDefaultMaxQueueSize = 5000;
inline const std::uint32_t kDefaultMaxConcurrentItems = 5000;
inline const std::uint32_t kDefaultMaxOperationRetries = 10;
inline const std::uint32_t kDefaultMaxReconnections = 10;
inline const bool kDefaultRequireMaster = true;
inline const auto kDefaultReconnectionDelay = std::chrono::milliseconds(100);
inline const auto kDefaultQueueTimeout = std::chrono::milliseconds(0);
inline const auto kDefaultOperationTimeout = std::chrono::seconds(7);
inline const auto kDefaultOperationTimeoutCheckPeriod = std::chrono::seconds(1);
inline const auto kTimerPeriod = std::chrono::milliseconds(200);
inline const std::uint32_t kMaxReadSize = 4096;
inline const std::uint32_t kDefaultMaxClusterDiscoverAttempts = 10;
inline const std::uint32_t kDefaultClusterManagerExternalHttpPort = 30778;
inline const std::uint32_t kCatchUpDefaultReadBatchSize = 500;
inline const std::uint32_t kCatchUpDefaultMaxPushQueueSize = 10000;

inline const auto kDefaultHeartbeatInterval = std::chrono::milliseconds(750);
inline const auto kDefaultHeartbeatTimeout = std::chrono::milliseconds(1500);
inline const auto kDefaultClientConnectionTimeout = std::chrono::milliseconds(1000);
inline const auto kDefaultGossipTimeout = std::chrono::seconds(1);

} // constants
} // connection
} // es

#endif // CONSTANTS_HPP