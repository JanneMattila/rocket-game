cmake_minimum_required(VERSION 3.10)
project(RocketServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add all source files
set(SOURCES
    RocketServer.cpp
    NetworkPacket.cpp
    CRC32.cpp
)

add_executable(RocketServer ${SOURCES})

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(RocketServer ws2_32)
endif()
