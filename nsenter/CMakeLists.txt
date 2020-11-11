add_library(nsenter nsenter.c)

target_include_directories(nsenter INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")