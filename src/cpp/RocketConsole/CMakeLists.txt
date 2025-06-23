cmake_minimum_required(VERSION 3.10)
project(RocketConsole)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static linking flags for Linux
if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
endif()

# Static CRT for MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Add all source files
set(SOURCES
    main.cpp
    Client.cpp
    ../RocketServer/Logger.cpp
    ../RocketServer/Utils.cpp
    ../RocketServer/CRC32.cpp
    ../RocketServer/NetworkPacket.cpp
    ../RocketServer/Network.cpp
    ../RocketServer/GamePacket.cpp
)

add_executable(RocketConsole ${SOURCES})

target_include_directories(RocketConsole
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../RocketServer
)

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(RocketConsole ws2_32)
endif()
