

# WebServer


- 并发模型是Proactor/Reactor + 线程池 + 非阻塞I/O，经Webbench测试支持上千并发连接.
- 使用状态机解析HTTP请求,支持解析GET和POST请求,支持优雅关闭连接.
- 使用基于最小堆的定时器关闭非活动连接.
- 使用单例模式实现同步日志系统/异步日志系统，支持日志按天分类，超行分类。
- 使用单例模式实现数据库连接池，通过CGI和同步线程分别完成web端的注册和登陆校验，


# 网站主页：![](https://img2020.cnblogs.com/blog/1755696/202007/1755696-20200703180145867-822248581.png)


# 环境配置: 使用QT内的gdb调试非常方便
```
cmake_minimum_required(VERSION 2.8)

project(web)

# 添加头文件路径
include_directories(
    "${PROJECT_SOURCE_DIR}/http"
    "${PROJECT_SOURCE_DIR}/log"
    "${PROJECT_SOURCE_DIR}/lock"
    "${PROJECT_SOURCE_DIR}/threadpool"
    "${PROJECT_SOURCE_DIR}/timer"
    "${PROJECT_SOURCE_DIR}/CGImysql"
    "${PROJECT_SOURCE_DIR}/config"

)

# 添加子目录
add_subdirectory(http)
add_subdirectory(log)
add_subdirectory(CGImysql)
add_subdirectory(timer)
add_subdirectory(config)

# 添加mysql库 pthread库
find_package (Threads)
find_package(PkgConfig)
pkg_check_modules(MySQL REQUIRED mysqlclient>=5.7)

add_executable(${PROJECT_NAME} "main.cpp" "webserver.cpp")
target_link_libraries(${PROJECT_NAME} http_function log_function cgimysql_function  config_function  timer_function)
target_link_libraries (${PROJECT_NAME}  ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} PUBLIC ${MySQL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${MySQL_LIBRARIES})

```


