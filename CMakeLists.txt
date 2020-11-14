cmake_minimum_required(VERSION 3.13)

project(systemd-wsl
        VERSION 0.0.1
        LANGUAGES C)

add_executable(systemd-wsl-helper systemd-wsl-helper.c)
# TODO:  use find_library to ensure we can find procps.
target_link_libraries(systemd-wsl-helper procps pam)