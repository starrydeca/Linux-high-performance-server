cmake_minimum_required(VERSION 3.15)
project(concurrent_CGI_server)
#源文件源文件路径
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC)
#头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)
#输出路径
set(EXECUTABLE_OUTPUT_PATH ..)
add_executable(concurrent_CGI_server ${SRC})
