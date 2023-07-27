/*
通过epoll实现服务器的IO复用，提升资源使用效率
*/ 
#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>  // epoll_ctl()控制epoll实例中感兴趣的文件描述符和事件类型
#include <fcntl.h>      // fcntl()用于对文件描述符进行各种控制操作
#include <unistd.h>     // close()
#include <assert.h>     // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    // 构造函数创建事件数组，同时创建epoll实例
    explicit Epoller(int maxEvent = 1024);

    ~Epoller(); // 析构只干了一件事，关闭了epoll实例

    // 添加文件描述符以及对于感兴趣的事件
    bool AddFd(int fd, uint32_t events);    

    // 修改文件描述符及其感兴趣的事件类型
    bool ModFd(int fd, uint32_t events);

    // 删除文件描述符
    bool DelFd(int fd);

    // 用于等待事件的发生，默认参数是-1代表不管超时，要直到有事件发生才返回
    int Wait(int timeoutMs = -1);

    // 获取事件数组中索引i下的文件描述符
    int GetEventFd(size_t i) const;

    // 获取事件数组中索引i下的事件
    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;   // epoll实例的文件描述符

    // epoll_event是Linux系统中用于描述事件的结构体，定义了这么一个数组
    std::vector<struct epoll_event> events_;    
};

#endif //EPOLLER_H