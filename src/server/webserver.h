/*
核心的webserver类
*/ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>      // fcntl()操作文件描述符
#include <unistd.h>     // close()
#include <assert.h>     // 包含断言
#include <errno.h>
#include <sys/socket.h> // socket bind等
#include <netinet/in.h> // 声明了网络字节序和主机字节序之间的转换函数
#include <arpa/inet.h>  // 包含了IP地址转换的相关函数

#include "epoller.h"    // epoller管理所有事件
#include "../log_system/log.h" // 日志打印
#include "../timer/heaptimer.h"     // 定时器
#include "../sql_connection_pool/sqlconnpool.h"    // 数据库连接池
#include "../threadpool/threadpool.h"     // 这里是一个线程池
#include "../sql_connection_pool/sqlconnRAII.h"    // 用户认证RAII
#include "../http/httpconn.h"       // http连接

// WebServer是一个整体的功能块的集合，这个功能块附带的功能有：
class WebServer {
public:
    WebServer(  // 构造函数
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();   // 析构

    void Start();   // 服务器开始启动的开关

private:
    // 创建监听的套接字以及一系列初始化行为
    bool InitSocket_();

    // 初始化事件模式(条件触发 or 边缘触发)
    void InitEventMode_(int trigMode);
    
    // 添加用于I/O的套接字，并绑定地址信息，这些信息将存入users_所对应的连接对象中
    void AddClient_(int fd, sockaddr_in addr);
    
    // 处理监听的套接字，为其生成用于I/O的套接字
    void DealListen_(); 
    // 处理写入(将服务器中的回应写入(返还)给客户端)
    void DealWrite_(HttpConn* client);
    // 处理读取(读取客户端的请求)
    void DealRead_(HttpConn* client);

    // 返回错误信息给客户端
    void SendError_(int fd, const char*info);

    // 给指定连接延迟时间/或者说重新设定超时时间
    void ExtentTime_(HttpConn* client);

    // 关闭某个连接
    void CloseConn_(HttpConn* client);

    // 读过程的处理
    void OnRead_(HttpConn* client);
    // 写过程的处理
    void OnWrite_(HttpConn* client);
    // 
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;    // 服务器能处理的最大连接数

    // 设置非阻塞模式
    static int SetFdNonblock(int fd);

    int port_;  // 端口
    bool openLinger_;   // 是否开启优雅关闭
    int timeoutMS_; // 毫秒MS
    bool isClose_;  // 服务器的连接状态
    int listenFd_;  // 监听的描述符
    char* srcDir_;  // 资源路径
    
    uint32_t listenEvent_;  // 监听事件
    uint32_t connEvent_;    // 连接事件
   
    std::unique_ptr<HeapTimer> timer_;  // 指向定时器的指针
    std::unique_ptr<ThreadPool> threadpool_;    // 指向线程池的指针
    std::unique_ptr<Epoller> epoller_;  // 指向epoller事件处理器的指针
    std::unordered_map<int, HttpConn> users_;   // 指向用户信息()的指针
};


#endif //WEBSERVER_H
