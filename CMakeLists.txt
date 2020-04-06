project(find-man-page)
cmake_minimum_required(VERSION 2.6)

add_subdirectory(cz)

include_directories(cz/include)
file(GLOB_RECURSE SRCS src/*.cpp)
add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} cz)
target_link_libraries(${PROJECT_NAME} z)
