cmake_minimum_required(VERSION 3.31.0)
project(opendds-publisher-v2 VERSION 0.1.0 LANGUAGES C CXX)

# Compiler options
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenDDS REQUIRED)

# ------------------------------------------------------------------
# ImGui sources
# ------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
# SDL3 for imgui backend
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
target_include_directories(imgui PUBLIC ${SDL3_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC ${SDL3_LIBRARIES} GL dl)

# ------------------------------------------------------------------
# UI sources
# ------------------------------------------------------------------
set(UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/main_loop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui.cpp
)
add_library(ui-objects OBJECT ${UI_SOURCES})
target_include_directories(ui-objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(ui-objects PUBLIC 
    imgui
    OpenDDS::Dcps
    OpenDDS::Tcp
    OpenDDS::Udp
    OpenDDS::Multicast
    OpenDDS::Rtps_Udp
    OpenDDS::InfoRepoDiscovery
)

add_executable(${PROJECT_NAME} 
    # cpp files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    $<TARGET_OBJECTS:ui-objects>
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# ------------------------------------------------------------------
# OpenDDS
# ------------------------------------------------------------------
file(GLOB IDL_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/idl/*.idl)
opendds_target_sources(${PROJECT_NAME} PUBLIC ${IDL_FILES} OPENDDS_IDL_OPTIONS -Gxtypes-complete -Lc++11)

target_link_libraries(${PROJECT_NAME} PRIVATE 
  imgui
  OpenDDS::Dcps
  OpenDDS::Tcp
  OpenDDS::Udp
  OpenDDS::Multicast
  OpenDDS::Rtps_Udp
  OpenDDS::InfoRepoDiscovery
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE IDL_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/idl/*.idl
)

set(GENERATED_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/src/generated.hpp) 

add_custom_command(
    OUTPUT ${GENERATED_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate.py ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate.py ${IDL_FILES}
    COMMENT "Generating header from IDL files"
    VERBATIM
)

add_custom_target(
    generate_script ALL
    DEPENDS ${GENERATED_HEADER}
)

add_dependencies(${PROJECT_NAME} generate_script)

# ------------------------------------------------------------------
# Conan Dependencies
# ------------------------------------------------------------------
# find_package(Boost CONFIG REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE Boost::boost)
# include_directories(${Boost_INCLUDE_DIR})
