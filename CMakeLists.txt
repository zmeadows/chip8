cmake_minimum_required(VERSION 3.10)

project(chip8 VERSION 0.4
              DESCRIPTION "A CHIP-8 emulator written in C++"
              LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CHIP8_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.hpp")
add_executable(chip8 ${CMAKE_SOURCE_DIR}/src/main.cpp ${CHIP8_SOURCES})

target_include_directories(chip8 PUBLIC src)
target_include_directories(chip8 PUBLIC "third_party")
target_include_directories(chip8 PUBLIC "${PROJECT_BINARY_DIR}")
configure_file(./src/chip8_config.h.in chip8_config.h)

add_definitions(-DCHIP8_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/")

# turn on most warnings and treat all warnings as errors
if (MSVC)
    target_compile_options(chip8 PUBLIC /W4 /WX)
else()
    target_compile_options(chip8 PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

find_package(OpenGL REQUIRED)

target_include_directories(chip8 PUBLIC "third_party/glew-2.1.0/include")
add_definitions(-DGLEW_STATIC)
add_subdirectory( ./third_party/glew-2.1.0 )
target_link_libraries( chip8 libglew_static )

target_include_directories(chip8 PUBLIC "third_party/glfw-3.3.2/include")
set( ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "Build basic example applications" )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_TESTS OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_INSTALL OFF CACHE BOOL  "GLFW lib only" )
add_subdirectory( ./third_party/glfw-3.3.2 )
target_link_libraries( chip8 glfw ${OPENGL_glu_LIBRARY} ${GLFW_LIBRARIES} )

target_include_directories(chip8 PUBLIC "third_party/cxxopts-2.2.0/include")

add_subdirectory( ./third_party/imgui-1.75 )
target_link_libraries(chip8 imgui)