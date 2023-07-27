/*
该头文件封装了所有的http连接模块，该模块将http的请求与响应做连接，中间桥梁
实现了连接的抽象化，定义化
*/
#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev分别读取/写入保存在多个缓冲区中的数据
#include <arpa/inet.h>   // sockaddr_in结构体包含了地址族、端口号、IP地址等信息
#include <stdlib.h>      // atoi()函数将字符串转为整数类型
#include <errno.h>      

#include "../log_system/log.h"
#include "../sql_connection_pool/sqlconnRAII.h"
#include "../data_buffer/buffer.h"
#include "httprequest.h"    // 连接请求模块的封装
#include "httpresponse.h"   // 连接回应模块的封装

class HttpConn {
public:
    // 构造函数初始化文件描述符，初始化地址，初始化连接状态
    HttpConn();

    // 析构函数的作用就是关闭连接
    ~HttpConn();

    // 该函数初始化连接
    void init(int sockFd, const sockaddr_in& addr);

    // 读取文件(套接字)描述符中的数据到缓冲区
    ssize_t read(int* saveErrno);   

    // 将缓冲区的数据写入到描述符
    ssize_t write(int* saveErrno);

    // 关闭连接
    void Close();

    int GetFd() const;      // 获取文件描述符

    int GetPort() const;    // 获取端口

    const char* GetIP() const;  // 获取IP
    
    sockaddr_in GetAddr() const;    // 获取地址簇信息
    
    // http连接针对http请求与http回应的处理
    bool process();

    // 获取要写入的长度
    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {  // 判断是否是持久连接
        return request_.IsKeepAlive();
    }

    static bool isET;   // epoll模式是边缘触发还是条件触发
    static const char* srcDir;  // 资源目录地址
    static std::atomic<int> userCount;  // 用户数量
    
private:
   
    int fd_;        // 服务端用于处理I/O的文件描述符
    struct  sockaddr_in addr_;  // 地址信息

    bool isClose_;  // 连接状态
    
    int iovCnt_;    // iov_结构体数组的长
    struct iovec iov_[2];   // 两个缓冲区，配合readv/writev使用
    
    Buffer readBuff_;   // 读缓冲区
    Buffer writeBuff_;  // 写缓冲区

    HttpRequest request_;   // 连接请求对象
    HttpResponse response_; // 连接回应对象
};


#endif //HTTP_CONN_H
