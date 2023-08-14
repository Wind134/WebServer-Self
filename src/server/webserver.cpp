/*
webserver的具体实现，有两个疑惑点：
- users_是如何初始化的；
- 监听套接字和连接套接字的机制分别是什么？
*/

#include "webserver.h"

using namespace std;

/**
 * @brief 服务器的构造函数，主要进行一些初始化操作；
 * @param port 服务器端口；
 * @param trigMode 触发模式(条件or边缘)；
 * @param timeoutMS 超时时间；
 * @param OptLinger 是否优雅关闭；
 * @param sqlPort 数据库端口；
 * @param sqlUser 数据库用户名；
 * @param sqlPwd 数据库密码；
 * @param sdName 数据库名；
 * @param connPoolNum 连接池数量；
 * @param threadNum 线程数量；
 * @param openLog 日志开关；
 * @param logLevel 日志等级；
 * @param logQueSize 日志队列大小；
 */
WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger, 
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    // 获取当前工作目录绝对路径，在终端的哪个地方运行程序，就获取哪个地方的目录
    // 后续考虑更改为指定目录的方式    
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);    // 要求目录存在
    // 路径拼接；限定了srcDir_长为16
    strncat(srcDir_, "/resources/", 16);

    HttpConn::userCount = 0;    // 用户连接的数量
    HttpConn::srcDir = srcDir_; // 给http资源目录赋路径

    // 初始化用户连接池实例
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);   // 初始化事件模式
    if(!InitSocket_()) { isClose_ = true;}  // 初始化成功，则表明连接已经建立

    if(openLog) {   // 如果开启了日志记录系统
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);   // 初始化
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }  // 如果连接没有正常开启
        else {  // 如果正常开启
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

/**
 * @brief 析构函数，关闭套接字，关闭(数据库)连接，释放内存空间；
 */
WebServer::~WebServer() {
    close(listenFd_);   // 关闭套接字
    isClose_ = true;    // 服务器设定为关闭状态
    free(srcDir_);  // 需要free吗？
    SqlConnPool::Instance()->ClosePool();   // 关闭数据库连接
}

/**
 * @brief 初始化事件处理模式(条件触发 or 边缘触发)
 * @param trigMode 指定的触发模式；
 */
void WebServer::InitEventMode_(int trigMode) {  // 初始化事件模式(条件触发 or 边缘触发)
    listenEvent_ = EPOLLRDHUP;  // 监听事件(对方关闭连接或者处于半关闭...)，这个时候要准备关闭连接
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // 连接事件同时关注两种状态(其中前者表示每个连接只触发一次事件，需要重新设置事件)
    switch (trigMode)
    {
    case 0: // 传入0，保持默认
        break;
    case 1: // 传入1，表明连接事件使用边缘触发模式
        connEvent_ |= EPOLLET;
        break;
    case 2: // 传入2，设置监听事件为边缘触发
        listenEvent_ |= EPOLLET;
        break;
    case 3: // 同时设置
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);    // 1 3可以保证使用边缘触发
}

/**
 * @brief 启动服务器；
 */
void WebServer::Start() {
    int timeMS = -1;  // 阻塞等待，初始值
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick(); // 获取下一个定时器的超时时间
        }
        int eventCnt = epoller_->Wait(timeMS);  // 等待，返回发生事件的数目(会按照数列索引的顺序逐个保存？)
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);   // 获取描述符
            uint32_t events = epoller_->GetEvents(i);   // 获取事件
            if(fd == listenFd_) {   // 如果刚好是要监听的描述符
                DealListen_();  // 处理监听
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 如果遇到连接中断，连接关闭，连接错误，则关闭连接
                assert(users_.count(fd) > 0);   // 这种情况下已经已经建立了连接，否则就是有问题；
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) { // 需要监听是否有进来的数据
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 处理读
            }
            else if(events & EPOLLOUT) {    // 需要监听是否有传给客户端的数据 
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    // 处理写
            } else {
                LOG_ERROR("Unexpected event");  // 否则就是一些预期之外的事件了
            }
        }
    }
}

/**
 * @brief 通过用于数据传输的套接字给客户端返回错误信息；
 * @param fd 连接套接字描述符；
 * @param info 错误信息，一个字符串数组；
 */
void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);  // 返回错误信息给客户端
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);    // 错误信息没有成功发送
    }
    close(fd);  // 因为出错了，所以关掉这个出错的文件描述符；
}

/**
 * @brief 关闭连接；
 * @param client 指向一个http连接的指针；
 */
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());   // 从例程中删除相应套接字
    client->Close();
}

/**
 * @brief 添加连接的客户端；
 * @param fd 要添加的处理与客户端进行数据传输的套接字描述符；
 * @param addr 传递的地址信息；
 */
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);  // 初始化http连接；
    if(timeoutMS_ > 0) {    // 每个客户端初始的等待时间
        // 下面这段代码传入了回调函数，该函数的作用是为了在超时后关闭某个连接
        // 然后从原理层面要说的是bind的三个参数，其中第二个参数this指针是函数参数中的隐式参数，第三个参数才是我们要用到的
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);  // 监听读事件以及其他一些自定义的连接事件
    SetFdNonblock(fd);  // 设置为非阻塞模式
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

/**
 * @brief 处理服务器的监听事件，准备与客户端连接，这个循环一直不间断的工作，直到用户数量超限；
 */
void WebServer::DealListen_() {
    struct sockaddr_in addr;    // 客户端的地址
    socklen_t len = sizeof(addr);   // 字节长
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); // 用于数据I/O的套接字
        if(fd <= 0) { return;}  // 失败，什么也不干
        else if(HttpConn::userCount >= MAX_FD) {    // 用户数量太多了，超过了最大能处理的连接
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);    // 监听事件如果设定为了边缘触发，则循环监听
}

/**
 * @brief 处理读事件；
 * @param client 指向一个http连接的指针；
 */
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);    // 更新超时时间
    // 用线程处理下面的读任务，实现并发处理
    threadpool_->submit(std::bind(&WebServer::OnRead_, this, client));
}

/**
 * @brief 处理写事件；
 * @param client 指向一个http连接的指针；
 */
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);    // 给响应的连接设置超时时间
    // 用线程处理下面的写入任务
    threadpool_->submit(std::bind(&WebServer::OnWrite_, this, client));
}

/**
 * @brief 延长超时时间；
 * @param client 指向要延长的http连接指针；
 */
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

/**
 * @brief 服务器的数据读取过程(但同时也包含了将要发送的信息写入缓冲区的过程)；
 * @param client 指向http连接的指针；
 */
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // http连接要读取的是来自客户端的请求；
    if(ret <= 0 && readErrno != EAGAIN) {   // EAGAIN表示阻塞，表示无法立即完成，但稍后可能成功；
        CloseConn_(client); // 关闭客户端，因为没读到；
        return;
    }
    OnProcess(client);  // 设定监视状态；
}

/**
 * @brief 针对http的处理结果做相应的监听操作；
 * @param client 指向http连接的指针；
 */
void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) { // 客户端将数据写入了缓冲区，根据这个环节成功与否处理监听方式
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 向缓冲区写入完成，准备发送了；
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);     // 写入没成功，因此还是需要关注读事件；
    }
}

/**
 * @brief 将缓冲区的数据发送到客户端；
 * @param client 指向http连接的指针；
 */
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {   // 如果写完了
        /* 传输完成 */
        if(client->IsKeepAlive()) { // 保持连接表明暂时先不断开
            OnProcess(client);  // 继续处理(即设定监听状态)
            return;
        }
    }
    else if(ret < 0) {  // 如果没写完，且返回的值异常
        if(writeErrno == EAGAIN) {  // 如果只是由于延迟而产生的异常，则试着继续写
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client); // 最终关闭连接
}

/**
 * @brief 初始化套接字连接；
 * @return 初始化的结果；
 */
bool WebServer::InitSocket_() {
    int ret;    // 承接各函数的返回值
    struct sockaddr_in addr;    // 地址族信息
    if(port_ > 65535 || port_ < 1024) { // 锁定端口
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }

    addr.sin_family = AF_INET;  // 设定地址族为IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 服务端随机分配IP，从主机字节序转换成网络字节序
    addr.sin_port = htons(port_);   // 将一个16位从主机字节序转换为网络字节序
    
    struct linger optLinger = { 0 };
    
    if(openLinger_) {   // 如果开启了优雅关闭
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;  // 启用
        optLinger.l_linger = 1; // 等1s后关闭
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);    // 通过socket函数创建服务器的监听套接字
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);   // 创建失败则记录日志
        return false;
    }

    // 设置套接字选项，启用优雅关闭
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {   // 如果设置失败
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1; // 1表明启用套接字复用
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定端口与ID
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));  // 给套接字描述符绑定地址信息
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6); // 等待连接请求状态(设置为可以使6个连接请求进入队列)
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    // 使用 listenEvent_ | EPOLLIN 的位运算操作可以将读取事件添加到已有的标志中，实现同时监听读取事件和其他事件的功能。
    // 如果没有使用位运算的或运算符|，而是直接使用单个事件标志，那么将只监听该单个事件，而不会同时监听其他事件。这可能会导致丢失其他事件的通知或处理。
    // 使用listenEvent_ | EPOLLIN的位运算操作的意义在于将读取事件添加到已有的epoll事件标志中，实现同时监听多个事件的功能。
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);  // epoll例程与套接字描述符绑定在一起
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);   // 这一步很细节，需要关闭这段
        return false;
    }

    SetFdNonblock(listenFd_);   // 设定为非阻塞模式

    LOG_INFO("Server port:%d", port_);  // 记录信息
    return true;
}

/**
 * @brief 给套接字描述符设定非阻塞模式；
 * @param fd 要设置的套接字描述符；
 */
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    // 下面这段是不是可以考虑简化？
    // 回答，不可以；
    // fcntl(fd, F_GETFD, 0)首先返回了原有的状态标志，就要这个原有的标志进行了添加；
    // 而下行代码是直接设置，忽略了原有标志； 
    // return fcntl(fd, F_SETFL, O_NONBLOCK);
    // 修复错误，需要获取的是套接字描述符的文件阻塞状态；F_GETFL->F_GETFD
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}