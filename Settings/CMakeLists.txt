cmake_minimum_required(VERSION 3.31)
project(Settings C)

set(CMAKE_C_STANDARD 11)  # GTK4 code is safer with C11

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

add_executable(Settings
        main.c
        keybinds.c
        waybar_presets.c
)

target_include_directories(Settings PRIVATE ${GTK4_INCLUDE_DIRS})
target_link_libraries(Settings PRIVATE ${GTK4_LIBRARIES})
target_compile_options(Settings PRIVATE ${GTK4_CFLAGS_OTHER})
