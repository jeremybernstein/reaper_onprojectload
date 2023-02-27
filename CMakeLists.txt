cmake_minimum_required(VERSION 3.10)

project(reaper_onprojectload
  VERSION
    1.0.1
  DESCRIPTION
    "Execute action on project load for REAPER"
  LANGUAGES
    CXX
)

set(SOURCES ./reaper_onprojectload.cpp)
set(LIBS "")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architecture" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "Minimum OS X deployment version" FORCE)
endif ()

if (WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(LIBS user32.lib)
endif ()

if (LINUX)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_definitions(-DNOMINMAX)
endif ()

add_library(onprojectload SHARED ${SOURCES})
target_include_directories(onprojectload PRIVATE ${INCLUDES} ../../WDL/WDL ../../sdk)
target_link_libraries(onprojectload ${LIBS})
set_target_properties(onprojectload PROPERTIES PREFIX "")
set_target_properties(onprojectload PROPERTIES OUTPUT_NAME "reaper_onprojectload")

if (APPLE)
    set_target_properties(onprojectload PROPERTIES SUFFIX ".dylib")
endif ()

if (WIN32)
    if ("${CMAKE_GENERATOR_PLATFORM}" MATCHES "Win32")
        set_target_properties(onprojectload PROPERTIES SUFFIX "_w32.dll")
    else ()
        set_target_properties(onprojectload PROPERTIES SUFFIX "_x64.dll")
    endif ()
endif ()
