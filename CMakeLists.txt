cmake_minimum_required(VERSION 3.8)
project(LightTCPVPN)

set(CMAKE_C_STANDARD 11)

add_subdirectory(
        simple-event
)

include_directories(
        .
        simple-event
)


add_executable(LightTCPVPN main.c src/tun.c src/tun.h src/TCPVPN.c src/TCPVPN.h)

target_link_libraries(LightTCPVPN simpleEventStatic)
target_link_libraries(LightTCPVPN hashTableStatic)
target_link_libraries(LightTCPVPN simpleTCPServerStatic)