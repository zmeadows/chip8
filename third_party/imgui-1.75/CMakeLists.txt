cmake_minimum_required (VERSION 3.5.1)
project (imgui VERSION 1.75)

file(GLOB IMGUI_HEADERS "${PROJECT_SOURCE_DIR}/*.h")
file(GLOB IMGUI_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")

add_library(imgui STATIC ${IMGUI_HEADERS} ${IMGUI_SOURCES})