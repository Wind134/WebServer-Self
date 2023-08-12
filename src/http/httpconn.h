/*
- 对HTTP请求报文的解析在httprequest.h头文件中完成；
- 对HTTP响应报文的发送在httpresponse.h头文件中处理，最终是写入到了服务器的内存缓冲区；
- 而头文件封装了所有的http连接方法，连接无法就做两方面的事，处理请求，发送响应；
- 该模块将http的请求与响应做连接，中间桥梁；
- 实现了连接的抽象化定义；
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
#include "httprequest.h"    // 请求报文的解析
#include "httpresponse.h"   // 响应报文的处理

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);   

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;
    
    sockaddr_in GetAddr() const;
    
    bool process();

    /**
     * @brief 返回要写入(套接字)的字节数，即便是条件触发，只要需要写入的字节数较多，就得重复处理，这是write函数的机制；
     * @return 待写入套接字描述符的数据长度；
     */
    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    /**
     * @brief 返回HTTP的持久连接状态；
     */
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;   // epoll模式是边缘触发还是条件触发
    static const char* srcDir;  // 资源目录地址
    static std::atomic<int> userCount;  // 用户数量
    
private:
   
    int fd_;        // 服务端用于与客户端连接通信的文件描述符
    struct  sockaddr_in addr_;  // 地址信息

    bool isClose_;  // 连接状态
    
    int iovCnt_;    // iov_结构体数组的长
    struct iovec iov_[2];   // 两个缓冲区，配合readv/writev使用
    
    /**
     * @brief 分配了两个缓冲区对象，一个是读缓冲区，一个写缓冲区；
     */
    Buffer readBuff_;   // 用来读取缓冲区中接收到的HTTP请求报文；
    Buffer writeBuff_;  // 用来向缓冲区写入要发送的响应报文；

    HttpRequest request_;   // 连接请求对象
    HttpResponse response_; // 连接回应对象
};

#endif //HTTP_CONN_H
