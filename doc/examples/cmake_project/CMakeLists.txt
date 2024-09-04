cmake_minimum_required(VERSION 3.13)  # CMake version check
project(app
LANGUAGES C Fortran
)

set(CMAKE_Fortran_FORMAT FREE)
add_executable(appc)
add_executable(appf)

find_package(DLB REQUIRED)
message(STATUS "using DLB VERSION: ${DLB_VERSION}")
target_sources(appc PRIVATE app.c)
target_sources(appf PRIVATE app.f90)

target_link_libraries(appc DLB::DLB)
target_link_libraries(appf DLB::DLB)
