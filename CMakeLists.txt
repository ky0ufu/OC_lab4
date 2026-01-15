cmake_minimum_required(VERSION 3.16)
project(temp_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(core
  src/timeutil.cpp
  src/retention.cpp
  src/agregator.cpp
  src/stdin_reader.cpp
  src/serial_reader.cpp
  src/serial_writer.cpp
)

target_include_directories(core PUBLIC src)

if (WIN32)
  target_compile_definitions(core PRIVATE NOMINMAX)
endif()

add_executable(temp_logger src/temp_logger_main.cpp)
target_link_libraries(temp_logger PRIVATE core)

add_executable(temp_simulator src/temp_simulator_main.cpp)
target_link_libraries(temp_simulator PRIVATE core)
