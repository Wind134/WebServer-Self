/*
webserver的具体实现
*/

#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,  // 设定端口，触发模式(条件or边缘)，延迟时间，(启用半关闭？)
            int sqlPort, const char* sqlUser, const  char* sqlPwd,  // 数据库端口，数据库用户名，数据库密码
            const char* dbName, int connPoolNum, int threadNum,     // 数据库名，数据库连接池数目，线程数目
            bool openLog, int logLevel, int logQueSize):            // ()，日志等级，日志队列大小
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    srcDir_ = getcwd(nullptr, 256); // 获取当前工作的目录绝对路径
    assert(srcDir_);    // 要求目录存在
    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;    // 用户连接的数量
    HttpConn::srcDir = srcDir_; // 给http资源目录赋路径
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);  // 一个用户连接池实例

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

WebServer::~WebServer() {
    close(listenFd_);   // 关闭套接字
    isClose_ = true;    // 服务器设定为关闭状态
    free(srcDir_);  // 需要free吗？
    SqlConnPool::Instance()->ClosePool();   // 关闭数据库连接
}

void WebServer::InitEventMode_(int trigMode) {  // 初始化事件模式(条件触发 or 边缘触发)
    listenEvent_ = EPOLLRDHUP;  // 监听事件(对方关闭连接或者处于半关闭...)
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
                assert(users_.count(fd) > 0);
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

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);  // 返回错误信息给客户端
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);    // 错误信息没有成功发送
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());   // 从例程中删除相应套接字
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);  // 初始化
    if(timeoutMS_ > 0) {
        // 下面这段代码传入了回调函数，该函数的作用是为了在超时后关闭某个连接
        // 然后从原理层面要说的是bind的三个参数，其中第二个参数this指针是函数参数中的隐式参数，第三个参数才是我们要用到的
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);  // 监听读事件以及其他一些自定义的连接事件
    SetFdNonblock(fd);  // 设置为非阻塞模式
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;    // 地址
    socklen_t len = sizeof(addr);   // 字节长
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); // 用于数据I/O的套接字
        if(fd <= 0) { return;}  // 失败，什么也不干
        else if(HttpConn::userCount >= MAX_FD) {    // 用户数量太多了，超过了做大能处理的连接
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);    // 更新超时时间
    // 用线程处理下面的读任务
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    // 用线程处理下面的写入任务
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) { // 延长时间，即设定超时
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // http连接要读取的是来自客户端的请求
    if(ret <= 0 && readErrno != EAGAIN) {   // EAGAIN表示阻塞，表示无法立即完成，但稍后可能成功
        CloseConn_(client); // 关闭客户端，因为没读到
        return;
    }
    OnProcess(client);  // 连接对象将读取套接字中的数据，传入到客户端
}

void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) { // 客户端去处理缓冲区的数据，如果处理成功
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 读取完成，可以写了
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);     // 读取没成功，因此还是需要关注读事件
    }
}

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
    CloseConn_(client); // 否则关闭连接
}

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
        optLinger.l_linger = 1; // 1s后关闭
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);    // 通过socket函数创建套接字
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

    int optval = 1;
    // 复用套接字，即便关闭也可以再延迟1s(源码设定)
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

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

int WebServer::SetFdNonblock(int fd) {  // 给套接字描述符设定非阻塞模式
    assert(fd > 0);
    // 下面这段是不是可以考虑简化
    // return fcntl(fd, F_SETFL, O_NONBLOCK);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


