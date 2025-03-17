cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(mpwm C)

set(CMAKE_C_STANDARD 11)

# find required packages
find_package(X11 REQUIRED)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(json-c CONFIG)

pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(XFT REQUIRED xft)

find_library(XI_LIBRARY Xi)
find_library(XINERAMA_LIBRARY Xinerama)
find_library(XRENDER_LIBRARY Xrender)
find_library(JSON_C_LIBRARY json-c)

include_directories(
    ${X11_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${XFT_INCLUDE_DIRS}
)

add_executable(mpwm 
    ./src/util.c
    ./src/common.c
    ./src/drw.c
    ./src/config.c
    ./src/cmds.c
    ./src/layouts.c
    ./src/resolvers.c
    ./src/events.c
    ./src/barwin.c
    ./src/devpair.c
    ./src/monitor.c
    ./src/client.c
    ./src/mpwm.c
)

target_compile_definitions(mpwm PUBLIC
    _DEFAULT_SOURCE
    _BSD_SOURCE
    _POSIX_C_SOURCE=2
    XINERAMA
    VERSION=\"2.0\"
    CONFIG_FILE=\".mpwm\"
)

target_link_libraries(mpwm 
    ${X11_LIBRARIES} 
    ${XI_LIBRARY} 
    ${XINERAMA_LIBRARY} 
    ${XRENDER_LIBRARY} 
    ${FREETYPE_LIBRARIES} 
    ${FONTCONFIG_LIBRARIES}
    ${XFT_LIBRARIES}
    ${JSON_C_LIBRARY}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(mpwm PRIVATE -g -DDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(mpwm PRIVATE)
endif()

target_compile_options(mpwm PRIVATE -O3 -march=native -pedantic -Wall -Wextra)
target_link_options(mpwm PRIVATE)

install(TARGETS mpwm DESTINATION /usr/local/bin)
