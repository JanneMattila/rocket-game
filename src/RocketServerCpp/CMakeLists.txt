cmake_minimum_required(VERSION 3.10)
project(RocketServerCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add all source files
set(SOURCES
    RocketServerCpp.cpp
    NetworkPacket.cpp
    CRC32.cpp
)

add_executable(RocketServerCpp ${SOURCES})

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(RocketServerCpp ws2_32)
endif()
