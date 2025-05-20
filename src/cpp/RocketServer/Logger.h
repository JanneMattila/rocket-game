#pragma once
#include <format>
#include <iomanip>
#include <map>
#include <sstream>
#include "LogLevel.h"

#define KV(name) {#name, std::to_string(name)}
#define KVS(name) {#name, name}
#define LOG_MAP(...) std::map<std::string, std::string>{ __VA_ARGS__ }

class Logger {
public:
    Logger();
    ~Logger();

    void SetLogLevel(LogLevel level);

    void Log(
        LogLevel level,
        const std::string& message,
        const std::map<std::string, std::string>& properties = {}
    );

private:
    LogLevel m_logLevel = LogLevel::DEBUG;

    std::string LogLevelToString(LogLevel level);
};
