cmake_minimum_required(VERSION 3.15)
project(test)

#设置文件路径
#set(SRC testaccept.cpp)
#搜索源文件
aux_source_directory(${PROJECT_SOURCE_DIR} SRC)
#指定输出路径
set(EXECUTABLE_OUTPUT_PATH ..)
#编译
add_executable(testaccept ${SRC})
