#pragma once

#ifndef USER_CREDENTIALS_HPP
#define USER_CREDENTIALS_HPP

#include <string>

namespace es {
namespace user {

class user_credentials
{
public:
	user_credentials() = default;
	explicit user_credentials(user_credentials const& other) = default;
	explicit user_credentials(user_credentials&& other) noexcept = default;
	user_credentials& operator=(user_credentials const& other) = default;

	explicit user_credentials(
		std::string const& username,
		std::string const& password
	)
		: username_(username), password_(password)
	{}

	explicit user_credentials(
		std::string&& username,
		std::string&& password
	)
		: username_(std::move(username)), password_(std::move(password))
	{}

	bool operator==(user_credentials const& rhs) const { return username_ == rhs.username_ && password_ == rhs.password_; }
	bool operator!=(user_credentials const& rhs) const { return !this->operator==(rhs); }
	bool null() const { return username_ == "" && password_ == ""; }
	std::string const& username() const { return username_; }
	std::string const& password() const { return password_; }

	void set_username(std::string const& username) { username_ = username; }
	void set_password(std::string const& password) { password_ = password; }
private:
	std::string username_;
	std::string password_;
};

} // user
} // es

#endif // USER_CREDENTIALS_HPP