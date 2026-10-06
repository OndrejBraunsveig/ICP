cmake_minimum_required(VERSION 4.0) 

# Version 4.0+ is needed to set working directory
# Make sure to use forward slashes (/) in the path!

project(icp VERSION 2026.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm           CONFIG REQUIRED)
find_package(GLEW                 REQUIRED)
find_package(glfw3         CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

if(WIN32)
    find_package(OpenCV CONFIG REQUIRED PATHS "C:/opencv/build")
    #for example: find_package(OpenCV CONFIG REQUIRED PATHS "C:/opencv/build")
elseif(UNIX)
    find_package(OpenCV CONFIG REQUIRED)
endif()

file(GLOB_RECURSE PROJECT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/face_detection.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# compile definitions and target properties can be set only AFTER add_executable(...)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
set_target_properties(${PROJECT_NAME} PROPERTIES
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

message(STATUS "My awesome project is using CMAKE SOURCE DIR: ${CMAKE_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} PRIVATE 
    GLEW::GLEW
    glfw
    glm::glm
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
    imgui::imgui
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)