#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace lab {

	// Logger manages the logging system for the engine, client, and asset converter.
	// Wrapper around spdlog to provide a simple interface for logging messages.
	class Logger {
	  public:
		// Sets the loggers and the patterns
		static void init();

		// Sets the level on all engine loggers. Accepts "trace", "debug", "info", "warn", "error", "critical", "off".
		// Unknown values fall back to "info" and emit a warning.
		static void setLevel(std::string_view level);

		// Returns the logger for the engine app
		[[nodiscard]] static std::shared_ptr<spdlog::logger>& getCoreLogger() {
			return m_Logger;
		}

	  private:
		static std::shared_ptr<spdlog::logger> m_Logger;
	};

} // namespace lab

// Log macros
#define CBK_TRACE(...) ::lab::Logger::getCoreLogger()->trace(__VA_ARGS__)
#define CBK_DEBUG(...) ::lab::Logger::getCoreLogger()->debug(__VA_ARGS__)
#define CBK_INFO(...) ::lab::Logger::getCoreLogger()->info(__VA_ARGS__)
#define CBK_WARN(...) ::lab::Logger::getCoreLogger()->warn(__VA_ARGS__)
#define CBK_ERROR(...) ::lab::Logger::getCoreLogger()->error(__VA_ARGS__)
#define CBK_FATAL(...) ::lab::Logger::getCoreLogger()->critical(__VA_ARGS__)
