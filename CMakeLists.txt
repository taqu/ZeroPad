cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME "ZeroPad")
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(HEADERS
    include/cpptoml.h
	include/controller.h
    include/keyboard.h
    include/mapper.h
)
set(SOURCES
    src/cpptoml.cpp
    src/controller.cpp
    src/keyboard.cpp
    src/mapper.cpp
)

source_group("include" FILES ${HEADERS})
source_group("src" FILES ${SOURCES})

if(WIN32)
    add_executable(${PROJECT_NAME} src/main.cpp ${SOURCES} ${HEADERS})
    target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_ROOT}/include)

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE Xinput9_1_0 User32)
else()
    message(STATUS "The ZeroPad application requires Windows; building portable mapper tests only.")
endif()

#if(BUILD_TESTING)
#    add_executable(mapper_tests tests/mapper_tests.cpp)
#    target_link_libraries(mapper_tests PRIVATE zeropad_mapper)
#    add_test(NAME mapper_tests COMMAND mapper_tests)
#endif()
