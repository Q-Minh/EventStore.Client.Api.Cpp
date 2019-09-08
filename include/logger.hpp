#pragma once

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string_view>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace es {

class logger
{
public:
	explicit logger(logger const& other) = delete;
	explicit logger(logger&& other) = delete;

	static logger& get() 
	{ 
		static logger logger_{};
		return logger_;
	}

	enum class level
	{
		trace,
		debug,
		info,
		warn,
		error,
		critical,
		off
	};

	logger::level get_level() const
	{ 
		switch (stdout_logger_->level())
		{
		case spdlog::level::trace:
			return level::trace;
		case spdlog::level::debug:
			return level::debug;
		case spdlog::level::info:
			return level::info;
		case spdlog::level::warn:
			return level::warn;
		case spdlog::level::err:
			return level::error;
		case spdlog::level::critical:
			return level::critical;
		case spdlog::level::off:
			return level::off;
		// impossible case, but compilers still issue warnings...
		default:
			return level::error;
		}
	}
		
	void set_level(logger::level lvl)
	{
		switch (lvl)
		{
		case level::trace:
			stdout_logger_->set_level(spdlog::level::trace);
			break;
		case level::debug:
			stdout_logger_->set_level(spdlog::level::debug);
			break;
		case level::info:
			stdout_logger_->set_level(spdlog::level::info);
			break;
		case level::warn:
			stdout_logger_->set_level(spdlog::level::warn);
			break;
		case level::error:
			stdout_logger_->set_level(spdlog::level::err);
			break;
		case level::critical:
			stdout_logger_->set_level(spdlog::level::critical);
			break;
		case level::off:
			stdout_logger_->set_level(spdlog::level::off);
			break;
		}
	}

	void set_level(std::string_view lvl)
	{
		if (lvl == "trace")
		{
			stdout_logger_->set_level(spdlog::level::trace);
		}
		else if (lvl == "debug")
		{
			stdout_logger_->set_level(spdlog::level::debug);
		}
		else if (lvl == "info")
		{
			stdout_logger_->set_level(spdlog::level::info);
		}
		else if (lvl == "warn")
		{
			stdout_logger_->set_level(spdlog::level::warn);
		}
		else if (lvl == "error")
		{
			stdout_logger_->set_level(spdlog::level::err);
		}
		else if (lvl == "critical")
		{
			stdout_logger_->set_level(spdlog::level::critical);
		}
		else if (lvl == "off")
		{
			stdout_logger_->set_level(spdlog::level::off);
		}
		else
		{ }
	}

	static void set_default_level(std::string_view lvl)
	{
		if (lvl == "trace")
		{
			spdlog::set_level(spdlog::level::trace);
		}
		else if (lvl == "debug")
		{
			spdlog::set_level(spdlog::level::debug);
		}
		else if (lvl == "info")
		{
			spdlog::set_level(spdlog::level::info);
		}
		else if (lvl == "warn")
		{
			spdlog::set_level(spdlog::level::warn);
		}
		else if (lvl == "error")
		{
			spdlog::set_level(spdlog::level::err);
		}
		else if (lvl == "critical")
		{
			spdlog::set_level(spdlog::level::critical);
		}
		else if (lvl == "off")
		{
			spdlog::set_level(spdlog::level::off);
		}
		else
		{
		}
	}

	template <class ...Args>
	void trace(std::string_view message, Args&&... args)
	{
		stdout_logger_->trace(message.data(), args...);
	}

	template <class ...Args>
	void debug(std::string_view message, Args&&... args)
	{
		stdout_logger_->debug(message.data(), args...);
	}

	template <class ...Args>
	void info(std::string_view message, Args&&... args)
	{
		stdout_logger_->info(message.data(), args...);
	}

	template <class ...Args>
	void warn(std::string_view message, Args&&... args)
	{
		stdout_logger_->warn(message.data(), args...);
	}

	template <class ...Args>
	void error(std::string_view message, Args&&... args)
	{
		stdout_logger_->error(message.data(), args...);
	}

	template <class ...Args>
	void critical(std::string_view message, Args&&... args)
	{
		stdout_logger_->critical(message.data(), args...);
	}

	template <class ...Args>
	void trace_sync(std::string_view message, Args&&... args)
	{
		spdlog::trace(message.data(), args...);
	}

	template <class ...Args>
	void debug_sync(std::string_view message, Args&&... args)
	{
		spdlog::debug(message.data(), args...);
	}

	template <class ...Args>
	void info_sync(std::string_view message, Args&&... args)
	{
		spdlog::info(message.data(), args...);
	}

	template <class ...Args>
	void warn_sync(std::string_view message, Args&&... args)
	{
		spdlog::warn(message.data(), args...);
	}

	template <class ...Args>
	void error_sync(std::string_view message, Args&&... args)
	{
		spdlog::error(message.data(), args...);
	}

	template <class ...Args>
	void critical_sync(std::string_view message, Args&&... args)
	{
		spdlog::critical(message.data(), args...);
	}

	void flush()
	{
		stdout_logger_->flush();
	}

	virtual ~logger()
	{
		spdlog::drop_all();
	}
private:
	explicit logger()
	{
		try
		{
			stdout_logger_ = spdlog::create_async<spdlog::sinks::stdout_color_sink_st>("stdout");
			stdout_logger_->set_level(spdlog::level::err);
		}
		catch (spdlog::spdlog_ex const& ex)
		{
			std::cerr << "failed to initialize logger : " << ex.what() << "\n";
		}
	}

	std::shared_ptr<spdlog::logger> stdout_logger_;
};

}

#define ES_TRACE(...) es::logger::get().trace_sync(__VA_ARGS__)
#define ES_DEBUG(...) es::logger::get().debug_sync(__VA_ARGS__)
#define ES_INFO(...) es::logger::get().info_sync(__VA_ARGS__)
#define ES_WARN(...) es::logger::get().warn_sync(__VA_ARGS__)
#define ES_ERROR(...) es::logger::get().error_sync(__VA_ARGS__)
#define ES_CRITICAL(...) es::logger::get().critical_sync(__VA_ARGS__)

// uncomment to get async logging
//#define ES_TRACE(...) es::logger::get().trace(__VA_ARGS__)
//#define ES_DEBUG(...) es::logger::get().debug(__VA_ARGS__)
//#define ES_INFO(...) es::logger::get().info(__VA_ARGS__)
//#define ES_WARN(...) es::logger::get().warn(__VA_ARGS__)
//#define ES_ERROR(...) es::logger::get().error(__VA_ARGS__)
//#define ES_CRITICAL(...) es::logger::get().critical(__VA_ARGS__)

#define ES_TRACE_SYNC(...) es::logger::get().trace_sync(__VA_ARGS__)
#define ES_DEBUG_SYNC(...) es::logger::get().debug_sync(__VA_ARGS__)
#define ES_INFO_SYNC(...) es::logger::get().info_sync(__VA_ARGS__)
#define ES_WARN_SYNC(...) es::logger::get().warn_sync(__VA_ARGS__)
#define ES_ERROR_SYNC(...) es::logger::get().error_sync(__VA_ARGS__)
#define ES_CRITICAL_SYNC(...) es::logger::get().critical_sync(__VA_ARGS__)

#define ES_DEFAULT_LOG_LEVEL(level) es::logger::get().set_level(level); es::logger::set_default_level(level);

#endif // LOGGER_HPP