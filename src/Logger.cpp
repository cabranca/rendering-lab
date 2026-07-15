#include "Logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace lab {

	std::shared_ptr<spdlog::logger> Logger::m_Logger;


	void Logger::init() {
		spdlog::set_pattern("´%^[%T] %n: %v%$");

		m_Logger = spdlog::stdout_color_mt("CBK");
		m_Logger->set_level(spdlog::level::info);
	}

	void Logger::setLevel(std::string_view level) {
		spdlog::level::level_enum lvl;
		if      (level == "trace")    lvl = spdlog::level::trace;
		else if (level == "debug")    lvl = spdlog::level::debug;
		else if (level == "info")     lvl = spdlog::level::info;
		else if (level == "warn")     lvl = spdlog::level::warn;
		else if (level == "error")    lvl = spdlog::level::err;
		else if (level == "critical") lvl = spdlog::level::critical;
		else if (level == "off")      lvl = spdlog::level::off;
		else {
			m_Logger->warn("Logger::setLevel: unknown level '{0}', falling back to 'info'", level);
			lvl = spdlog::level::info;
		}

		m_Logger->set_level(lvl);
	}

} // namespace lab
