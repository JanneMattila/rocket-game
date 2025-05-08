#include "Logger.h"
#include <iomanip>

// Constructor
Logger::Logger() : m_logLevel(LogLevel::INFO), m_fileLoggingEnabled(false) {}

// Destructor
Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

// Set the logging level
void Logger::SetLogLevel(LogLevel level) {
    m_logLevel = level;
}

// Enable logging to a file
void Logger::EnableFileLogging(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFile.open(filePath, std::ios::out | std::ios::app);
    m_fileLoggingEnabled = m_logFile.is_open();
}

// Disable logging to a file
void Logger::DisableFileLogging() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_fileLoggingEnabled = false;
}

// Get the current timestamp
std::string Logger::GetTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Convert LogLevel to string
std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::EXCEPTION: return "EXCEPTION";
    default: return "UNKNOWN";
    }
}
