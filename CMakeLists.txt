cmake_minimum_required(VERSION 3.13)

project(systemd-wsl
        VERSION 0.0.1
        LANGUAGES C)

add_subdirectory(nsenter)

add_executable(systemd-namespace main.c)
target_link_libraries(systemd-namespace nsenter)