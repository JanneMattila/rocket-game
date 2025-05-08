#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ctime>
#include <sstream>

enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARNING,
	EXCEPTION
};
