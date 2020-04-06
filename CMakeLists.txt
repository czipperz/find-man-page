project(find-man-page)
cmake_minimum_required(VERSION 2.6)

add_subdirectory(cz)
include_directories(cz/include)

add_executable(find-man-page src/find-man-page.cpp)
target_link_libraries(find-man-page cz)
target_link_libraries(find-man-page z)

add_executable(autocomplete-man-page src/autocomplete-man-page.cpp)
target_link_libraries(autocomplete-man-page cz)
target_link_libraries(autocomplete-man-page z)

add_executable(load-man-page src/load-man-page.cpp)
target_link_libraries(load-man-page cz)
target_link_libraries(load-man-page z)
