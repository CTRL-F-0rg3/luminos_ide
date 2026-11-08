cmake_minimum_required(VERSION 3.10)
project(LuminosCompositor LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

set(XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")
set(XDG_SHELL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
set(XDG_SHELL_CODE "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")

# Generowanie plików
add_custom_command(
    OUTPUT ${XDG_SHELL_HEADER} ${XDG_SHELL_CODE}
    COMMAND wayland-scanner client-header ${XDG_SHELL_XML} ${XDG_SHELL_HEADER}
    COMMAND wayland-scanner private-code ${XDG_SHELL_XML} ${XDG_SHELL_CODE}
    DEPENDS ${XDG_SHELL_XML}
    COMMENT "Generating Wayland xdg-shell protocol files"
)

include_directories(include)
include_directories(${WAYLAND_CLIENT_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# KLUCZOWE: dodajemy wygenerowany nagłówek do źródeł, żeby CMake wiedział o zależności
add_executable(luminos-compositor 
    src/main.cpp 
    ${XDG_SHELL_HEADER}  # <-- To wymusza generowanie przed kompilacją
    ${XDG_SHELL_CODE}
)

target_link_libraries(luminos-compositor PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
