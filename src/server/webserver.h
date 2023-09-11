/*
头文件介绍：
- 服务器的完整框架，综合运用之前设计的所有模块；
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
#include "../threadpool/threadpool.h"     // 线程池
#include "../sql_connection_pool/sqlconnRAII.h"    // 用户认证RAII
#include "../http/httpconn.h"       // http连接处理

// WebServer是一个整体的功能块的集合，这个功能块附带的功能有：
class WebServer {
public:
    WebServer(  // 构造函数
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();   // 析构

    void Start();

private:
    bool InitSocket_();

    void InitEventMode_(int trigMode);

    void DealListen_();

    void OnProcess(HttpConn* client);
    
    void AddClient_(int fd, sockaddr_in addr); 

    void OnRead_(HttpConn* client);

    void OnWrite_(HttpConn* client);

    void DealRead_(HttpConn* client);

    void DealWrite_(HttpConn* client);

    void SendError_(int fd, const char*info);

    void ExtentTime_(HttpConn* client);

    void CloseConn_(HttpConn* client);

    static const int MAX_FD = 65536;    // 服务器能处理的最大连接数

    // 设置非阻塞模式
    static int SetFdNonblock(int fd);

    int port_;  // 端口
    bool openLinger_;   // 是否开启优雅关闭
    int timeoutMS_; // 毫秒MS
    bool isClose_;  // 服务器的连接状态
    int listenFd_;  // 监听的描述符
    char* srcDir_;  // 资源路径
    
    uint32_t listenEvent_;  // 监听事件；
    uint32_t connEvent_;    // 连接事件；
   
    std::unique_ptr<HeapTimer> timer_;  // 指向定时器的指针；
    std::unique_ptr<ThreadPool> threadpool_;    // 指向线程池的指针；
    std::unique_ptr<Epoller> epoller_;  // 指向epoller事件处理器的指针；
    std::unordered_map<int, HttpConn> users_;   // 套接字<->HTTP连接，映射的下标操作会进行value的默认构造；
};


#endif //WEBSERVER_H
