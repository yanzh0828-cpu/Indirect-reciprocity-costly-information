cmake_minimum_required(VERSION 3.16)
project(indirect_reciprocity CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -funroll-loops")

find_package(OpenMP REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQL_LIBRARY mysqlclient)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
  message(FATAL_ERROR "MySQL client development files not found (install libmysqlclient-dev)")
endif()

add_executable(ir_sim
  src/main.cpp
  src/simulation.cpp
  src/db.cpp
)

target_include_directories(ir_sim PRIVATE src ${MYSQL_INCLUDE_DIR})
target_link_libraries(ir_sim PRIVATE OpenMP::OpenMP_CXX ${MYSQL_LIBRARY})
