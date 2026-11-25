cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(jump-cue VERSION 1.0.0)

# Every .cpp inside src/ (recursively) is compiled into the mod.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "No se encontro el Geode SDK. Define la variable de entorno GEODE_SDK "
        "apuntando a la carpeta del SDK (por ejemplo C:/Geode/sdk) o abre el "
        "proyecto desde el Geode CLI / VS Code con la extension de Geode.")
endif()

message(STATUS "Usando Geode SDK: $ENV{GEODE_SDK}")
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
