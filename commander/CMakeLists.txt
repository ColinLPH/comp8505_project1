cmake_minimum_required(VERSION 3.16)
project(commander VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_executable(commander ${SRC_FILES})

target_include_directories(commander
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Compiler warning flags
if(MSVC)
  target_compile_options(commander PRIVATE /W4)
else()
  target_compile_options(commander PRIVATE -Wall -Wextra -Wpedantic)
  # Link with libcrypt on Unix/Linux if needed
  # target_link_libraries(commander PRIVATE crypt)
endif()
