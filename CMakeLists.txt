cmake_minimum_required(VERSION 3.10)    # 最低要求版本

project(WebServer-Self)     # 项目名

set(CMAKE_CXX_STANDARD 14)  # 启用c++ 14标准

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)   # 可执行程序路径

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModule/;${CMAKE_MODULE_PATH};") # 设置CMake模块路径

find_package(Threads REQUIRED)  # 线程库
find_package(MySQL REQUIRED)    # MySQL库

aux_source_directory(./src/data_buffer BUFFER)
aux_source_directory(./src/http HTTP)
aux_source_directory(./src/log_system LOG)
aux_source_directory(./src/server SERVER)
aux_source_directory(./src/sql_connection_pool SQL_CONN_POOL)
aux_source_directory(./src/threadpool THREADPOOL)
aux_source_directory(./src/timer TIMER)

set(ALL_SOURCES ${BUFFER} ${HTTP} ${LOG} ${SERVER} ${SQL_CONN_POOL} ${THREADPOOL} ${TIMER}) # 合在一处

add_executable(WebServer-Self ${ALL_SOURCES} ${PROJECT_SOURCE_DIR}/src/main.cpp)

include_directories(${MYSQL_INCLUDE_DIR})   # 会在FindMySQL.cmake中寻找MySQL的支持库
target_link_libraries(WebServer-Self PUBLIC Threads::Threads ${MYSQL_LIB})    # 添加线程以及MySQL相关的库，链接需要使用到
