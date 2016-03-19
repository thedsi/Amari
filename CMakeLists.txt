cmake_minimum_required(VERSION 3.2.2)
project(amari)

find_package(Qt5 COMPONENTS Core Widgets Gui Concurrent REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

file(GLOB HEADERS src/*.h)
file(GLOB SOURCES src/*.cpp)

add_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)

add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::Concurrent)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

if(STATIC_RELEASE)
        
    # Force Relase build
    set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "CMAKE_CONFIGURATION_TYPES" FORCE)
    
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_RELEASE)
    
        # Undef CRT
        string(REGEX REPLACE "/MD[^ ]*" "" ${flag_var} "${${flag_var}}")
        
        # Undef Exception Handling
        string(REGEX REPLACE "/EH[^ ]*" "" ${flag_var} "${${flag_var}}")
        
        # Undef Enable RTTI
        string(REGEX REPLACE "/GR[^ ]*" "" ${flag_var} "${${flag_var}}")
        
        # Undef Enable Stack Protector
        string(REGEX REPLACE "/GS[^ ]*" "" ${flag_var} "${${flag_var}}")
        
    endforeach()
    
    # Static CRT, Optimize for Size, Disable RTTI, Disable Stack Protector, Enable LTCG
    # No exception handling (no /EHsc and /D_HAS_EXCEPTIONS=0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT /GR- /GS- /GL /D_HAS_EXCEPTIONS=0")
    
    add_definitions(-DSTATIC_RELEASE)
    
    find_library(L_QTPCRE qtpcre)
    if(L_QTPCRE)
        target_link_libraries(${PROJECT_NAME} "${L_QTPCRE}")
    else()
        message("${L_QTPCRE}")
    endif()
    
    find_library(L_QTPS Qt5PlatformSupport)
    if(L_QTPS)
        target_link_libraries(${PROJECT_NAME} "${L_QTPS}")
    else()
        message("${L_QTPS}")
    endif()
    
    target_link_libraries(${PROJECT_NAME} Qt5::QWindowsIntegrationPlugin opengl32 wsock32 imm32 winmm)
    
endif()
