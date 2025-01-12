cmake_minimum_required(VERSION 3.14)
project(MyWebServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 调试模式
option(DEBUG "Enable debug mode" ON)
if (DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# 包含头文件目录
include_directories(
    include
    CGImysql
    http
    lock
    log
    threadpool
    timer
)

# 添加主项目
add_executable(server
    main.cpp
    webserver.cpp
    config.cpp
    timer/lst_timer.cpp
    http/http_conn.cpp
    log/log.cpp
    CGImysql/sql_connection_pool.cpp
)

# 链接库
target_link_libraries(server pthread mysqlclient)

# 包含 test 目录
add_subdirectory(test)

# 启用测试失败时输出日志
set(CTEST_OUTPUT_ON_FAILURE ON)

# 添加一个自定义目标来运行 ctest 并显示详细输出
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests with detailed output"
)

# 将 run_all_tests 目标与 make test 关联
add_custom_target(test DEPENDS run_all_tests)