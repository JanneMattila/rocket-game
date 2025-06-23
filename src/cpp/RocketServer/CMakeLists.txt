cmake_minimum_required(VERSION 3.10)
project(RocketServer)

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
    NetworkPacket.cpp
    CRC32.cpp
    GamePacket.cpp
    Logger.cpp
    Server.cpp
    Network.cpp
    Utils.cpp
)

add_executable(RocketServer ${SOURCES})

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(RocketServer ws2_32)
endif()
