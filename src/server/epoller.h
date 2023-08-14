/*
头文件介绍：
- 通过epoll实现服务器的IO复用，提升资源使用效率；
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
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);    

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    int Wait(int timeoutMs = -1);

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;   // epoll实例的文件描述符

    // epoll_event是Linux系统中用于描述事件的结构体，定义了这么一个结构体数组；
    // 该结构体包含数据与用户信息等内容；
    std::vector<struct epoll_event> events_;    
};

#endif //EPOLLER_H