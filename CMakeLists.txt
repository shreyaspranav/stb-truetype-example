cmake_minimum_required(VERSION 3.5)

add_subdirectory(deps/glfw)

project(stb_truetype_example)
add_executable(stb_truetype_example Main.cpp)

target_link_libraries(stb_truetype_example PUBLIC glfw)

target_include_directories(stb_truetype_example PUBLIC 
    deps/glfw/include
    deps/glad/include
    deps/glm/include
    deps/stb)

add_custom_command(
    TARGET stb_truetype_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/fonts/
            ${CMAKE_CURRENT_BINARY_DIR}/fonts/)