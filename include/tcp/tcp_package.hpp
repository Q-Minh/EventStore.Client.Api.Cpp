#pragma once

#ifndef TCP_PACKAGE_HPP
#define TCP_PACKAGE_HPP

#include <string>
#include <string_view>

#include "guid.hpp"

#include "tcp/tcp_commands.hpp"
#include "tcp/tcp_flags.hpp"

namespace es {
namespace detail {
namespace tcp {

const int kLengthOffset = 0 /* sizeof(unsigned int) */;
const int kCommandOffset = kLengthOffset + 4;
const int kFlagsOffset = kCommandOffset + 1;
const int kCorrelationOffset = kFlagsOffset + 1;
const int kAuthOffset = kCorrelationOffset + 16;
const int kMandatorySize = kAuthOffset;

const int kMaxPackageSize = 64 * 1024 * 1024;

class tcp_package_view
{
public:
	tcp_package_view() = default;

	explicit tcp_package_view(
		std::byte* package,
		std::size_t length
	) : package_(package), length_(length - 4)
	{
		if (length < kMandatorySize) package_ = nullptr;

		if (package_ != nullptr && is_authenticated())
		{
			std::uint8_t login_size = (std::uint8_t)package_[kAuthOffset];
			auto password_offset = kAuthOffset + 1 + login_size;
			std::uint8_t password_size = (std::uint8_t)package_[password_offset];

			if (length < (kMandatorySize + 1 + login_size + 1 + password_size))
			{
				package_ = nullptr;
			}
		}
	}

	tcp_command command() const { return *reinterpret_cast<tcp_command*>(&package_[kCommandOffset]); }
	tcp_flags flags() const { return *reinterpret_cast<tcp_flags*>(&package_[kFlagsOffset]); }
	std::string_view correlation_id() const { return std::string_view(reinterpret_cast<char*>(&package_[kCorrelationOffset]), 16); }
	bool is_valid() const { return package_ != nullptr; }
	bool is_authenticated() const
	{
		return flags() == tcp_flags::authenticated;
	}
	std::uint8_t login_size() const { return (std::uint8_t)package_[kAuthOffset]; }
	unsigned int login_offset() const
	{
		return kAuthOffset + 1;
	}
	unsigned int password_offset() const
	{
		auto password_size_offset = login_offset() + login_size();
		return password_size_offset + 1;
	}
	std::uint8_t password_size() const
	{
		auto password_size_offset = login_offset() + login_size();
		return (std::uint8_t)package_[password_size_offset];
	}

	// if is_authenticated() is false, undefined behavior
	std::string_view login() const
	{
		return std::string_view(reinterpret_cast<char*>(&package_[login_offset()]), (std::size_t)login_size());
	}
	// if is_authenticated() is false, undefined behavior
	std::string_view password() const
	{
		return std::string_view(reinterpret_cast<char*>(&package_[password_offset()]), (std::size_t)password_size());
	}
	unsigned int message_offset() const
	{
		if (is_authenticated())
		{
			return password_offset() + password_size();
		}
		else
		{
			return kMandatorySize;
		}
	}
	unsigned int message_size() const
	{
		return static_cast<unsigned int>(length_ + 4 - message_offset());
	}
	char* data() { return (char*)package_; }
	std::size_t size() const { return length_ + 4; }

private:
	std::byte* package_;
	std::size_t length_;
};

template <class Allocator = std::allocator<std::byte>>
class tcp_package
{
public:
	tcp_package(tcp_package&& other)
		: package_(other.package_), length_(other.length_), alloc_(other.alloc_)
	{
		// check alloc for propagate on move/copy, and all of that stuff...

		// deallocating on a nullptr should be a noop
		other.package_ = nullptr;
	}
	tcp_package& operator=(tcp_package&& other)
	{
		package_ = other.package_;
		length_ = other.length_;
		// check for propagate on move/copy, and all of that stuff...
		alloc_ = other.alloc_;

		// deallocating on a nullptr should be a noop
		other.package_ = nullptr;
	}
	tcp_package(tcp_package const& other)
		: package_(nullptr), length_(other.length_), alloc_(other.alloc_)
	{
		package_ = alloc_.allocate(length_ + 4);
		std::memcpy(package_, other.package_, other.length_ + 4);
	}
	tcp_package& operator=(tcp_package const& other)
	{
		length_ = other.length_;
		alloc_ = other.alloc_;
		package_ = alloc_.allocate(length_ + 4);
		std::memcpy(package_, other.package_, other.length_ + 4);
	}

	// unauthenticated ctor
	explicit tcp_package(
		tcp_command command,
		tcp_flags flags,
		guid_type const& correlation_id,
		std::byte* message = nullptr,
		std::size_t length = 0,
		Allocator alloc = Allocator()
	)
		: tcp_package(command, flags, std::string_view((char*)correlation_id.data, correlation_id.size()), message, length, alloc)
	{}

	explicit tcp_package(
		tcp_command command,
		tcp_flags flags,
		std::string_view correlation_id,
		std::byte* message = nullptr,
		std::size_t length = 0,
		Allocator alloc = Allocator()
	) : package_(nullptr),
		length_(0),
		alloc_(alloc)
	{
		if ((char)flags & (char)tcp_flags::authenticated) return;

		length_ = 18 + length; // command, flag, correlation id, message
		package_ = (std::byte*)alloc_.allocate(length_ + 4);
		// write length of message
		*reinterpret_cast<int*>(&package_[kLengthOffset]) = (unsigned int)length_;
		// write command
		*reinterpret_cast<tcp_command*>(&package_[kCommandOffset]) = command;
		// write flags
		*reinterpret_cast<tcp_flags*>(&package_[kFlagsOffset]) = flags;
		// write correlation id
		std::memcpy(&package_[kCorrelationOffset], correlation_id.data(), correlation_id.size());
		// write message
		std::memcpy(&package_[kMandatorySize], message, length);
	}

	explicit tcp_package(
		tcp_command command,
		tcp_flags flags,
		std::string_view correlation_id,
		std::string const& login,
		std::string const& password,
		std::byte* message = nullptr,
		std::size_t length = 0,
		Allocator alloc = Allocator()
	) : package_(nullptr),
		length_(0),
		alloc_(alloc)
	{
		if (!((char)flags & (char)tcp_flags::authenticated)) return;

		// command, flag, correlation id, login_size, login, password_size, password, message
		length_ = 18 + 1 + login.size() + 1 + password.size() + length;
		package_ = (std::byte*)alloc_.allocate(length_ + 4);
		// write length of message
		*reinterpret_cast<int*>(&package_[kLengthOffset]) = (unsigned int)length_;
		// write command
		*reinterpret_cast<tcp_command*>(&package_[kCommandOffset]) = command;
		// write flags
		*reinterpret_cast<tcp_flags*>(&package_[kFlagsOffset]) = flags;
		// write correlation id
		std::memcpy(&package_[kCorrelationOffset], correlation_id.data(), correlation_id.size());
		// write login_size + login
		*reinterpret_cast<std::uint8_t*>(&package_[kAuthOffset]) = (std::uint8_t)login.size();
		std::memcpy(&package_[kAuthOffset + 1], login.data(), login.size());
		// write password_size + password
		auto password_size_offset = kAuthOffset + 1 + login.size();
		*reinterpret_cast<std::uint8_t*>(&package_[password_size_offset]) = static_cast<std::uint8_t>(password.size());
		std::memcpy(&package_[password_size_offset + 1], password.data(), password.size());

		// write message
		auto message_offset = password_size_offset + 1 + password.size();
		std::memcpy(&package_[message_offset], message, length);
	}

	// authenticated ctor
	explicit tcp_package(
		tcp_command command,
		tcp_flags flags,
		guid_type const& correlation_id,
		std::string const& login,
		std::string const& password,
		std::byte* message = nullptr,
		std::size_t length = 0,
		Allocator alloc = Allocator()
	)
		: tcp_package(command, flags, std::string_view((char*)correlation_id.data, correlation_id.size()), login, password, message, length, alloc)
	{}

	void change_length_endianness()
	{
		std::swap(package_[0], package_[3]);
		std::swap(package_[1], package_[2]);
	}
	bool is_valid() const { return package_ != nullptr && length_ >= 18 && is_header_valid(); }
	bool is_header_valid() const { return *reinterpret_cast<int*>(&package_[kLengthOffset]) == static_cast<int>(length_); }
	char* data() { return (char*)package_; }
	std::size_t size() const { return length_ + 4; }
	
	// implicit conversion
	operator tcp_package_view() const { return tcp_package_view(package_, length_ + 4); }
	// explicit conversion
	explicit operator tcp_package_view*() const { return tcp_package_view(package_, length_ + 4); }

	~tcp_package()
	{
		alloc_.deallocate(package_, length_ + 4);
	}
private:
	std::byte* package_;
	std::size_t length_;
	Allocator alloc_;
};

} // detail
} // tcp
} // es

#endif // TCP_PACKAGE_HPP