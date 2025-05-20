#include "Logger.h"
#include <iostream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::mutex s_mutex;

Logger::Logger() {}
Logger::~Logger() {}

void Logger::SetLogLevel(LogLevel level)
{
    m_logLevel = level;
}

void Logger::Log(
    LogLevel level,
    const std::string& message,
    const std::map<std::string, std::string>& properties
)
{
    if (level < m_logLevel) return;

    std::time_t now = std::time(nullptr);
    std::tm utcTime;
#ifdef _WIN32
    gmtime_s(&utcTime, &now);
#else
    gmtime_r(&now, &utcTime);
#endif

    try
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        std::ostringstream oss;
        oss
            << "{"
            << "\"timestamp\":\"" << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ") << "\","
            << "\"severity\":\"" << LogLevelToString(level) << "\","
            << "\"message\":\"" << message << "\"";
        for (const auto& kv : properties) {
            oss << ",\"" << kv.first << "\":\"" << kv.second << "\"";
        }
        oss << "}";
        std::cout << oss.str() << std::endl;
    }
    catch (const std::exception&)
    {

    }

}

std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::EXCEPTION: return "EXCEPTION";
    default: return "UNKNOWN";
    }
}
