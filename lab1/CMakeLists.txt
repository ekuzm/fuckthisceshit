cmake_minimum_required(VERSION 3.16)
project(power_widget VERSION 1.0 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Эта лабораторная работа предназначена только для Linux")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET gio-2.0>=2.52)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_executable(power_widget
    src/main.cpp
    src/ui.cpp
    src/logic/power.cpp
    src/logic/state.cpp
    src/logic/format.cpp
    src/logic/journal.cpp
    src/logic/monitoring.cpp
    src/logic/sleep.cpp
    src/logic/autostart.cpp
)
target_include_directories(power_widget PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(power_widget PRIVATE cxx_std_17)
target_compile_options(power_widget PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(power_widget PRIVATE PkgConfig::GTK PkgConfig::GLIB PkgConfig::UDEV PkgConfig::SYSTEMD)
install(TARGETS power_widget RUNTIME DESTINATION bin)
