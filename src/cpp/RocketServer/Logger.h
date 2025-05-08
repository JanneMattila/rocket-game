#pragma once
#include <format>
#include "LogLevel.h"

class Logger {
public:
    Logger();
    ~Logger();

    void SetLogLevel(LogLevel level);
    void EnableFileLogging(const std::string& filePath);
    void DisableFileLogging();
    // Log a message
    template <typename... Args>
    void Log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < m_logLevel) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        // Format the message
        std::string message = std::vformat(format, std::make_format_args(std::forward<Args>(args)...));

        std::string logMessage = GetTimestamp() + " [" + LogLevelToString(level) + "] " + message;

        // Log to console
        std::cout << logMessage << std::endl;

        // Log to file if enabled
        if (m_fileLoggingEnabled && m_logFile.is_open()) {
            m_logFile << logMessage << std::endl;
        }
    };

private:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string GetTimestamp();
    std::string LogLevelToString(LogLevel level);

    LogLevel m_logLevel;
    std::ofstream m_logFile;
    bool m_fileLoggingEnabled;
    std::mutex m_mutex;
};
