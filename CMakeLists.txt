cmake_minimum_required(VERSION 2.8.4)
project(nsnake)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)

add_executable(nsnake nsnake.cpp)
target_link_libraries(nsnake ${CURSES_LIBRARIES})
install(TARGETS nsnake DESTINATION bin)
