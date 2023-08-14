/*
连接模块的具体实现，其中处理请求与相应的方法即为process方法
*/
#include "httpconn.h"
using namespace std;

/**
 * @brief 下面三个变量编译器会执行默认初始化(C++特有的功能)；
 */
const char* HttpConn::srcDir;   // 资源路径，默认初始化为nullptr
std::atomic<int> HttpConn::userCount;   // 用户数量，原子变量，操作它的时候不能干扰，默认初始化为0
bool HttpConn::isET;    // 边缘触发还是条件触发，默认初始化为false

/**
 * @brief 构造函数初始化套接字描述符，地址信息，连接状态；
 */
HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

/**
 * @brief 析构函数的任务就是关闭HTTP连接；
 */
HttpConn::~HttpConn() { 
    Close(); 
};

/**
 * @brief 初始化HTTP连接；
 * @param fd 套接字描述符；
 * @param add 地址信息；
 */
void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    ++userCount;    // 更新用户数量
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();   // 清空读缓冲区所有字符
    readBuff_.RetrieveAll();    // 清空写缓冲区所有字符
    isClose_ = false;           // 更改连接状态

    // 打印日志信息
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

/**
 * @brief 关闭HTTP连接；
 */
void HttpConn::Close() {
    response_.UnmapFile();  // 首先解除响应报文中文件内容的映射
    if(isClose_ == false){  // 如果是连接着的状态
        isClose_ = true;    // 更新状态为关闭状态
        --userCount;        // 用户数量减1
        close(fd_);         // 关闭套接字
        // 打印日志
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

/**
 * @brief 返回套接字描述符；
 * @return 套接字描述符；
 */
int HttpConn::GetFd() const {
    return fd_;
};

/**
 * @brief 返回地址信息，常量版本的函数；
 * @return 地址信息；
 */
struct sockaddr_in HttpConn::GetAddr() const {  // 返回地址信息
    return addr_;
}

/**
 * @brief 将一个IPv4地址转换为一个点分十进制字符串；
 * @return 字符串形式的IP地址；
 */
const char* HttpConn::GetIP() const {   // 
    return inet_ntoa(addr_.sin_addr);   
}

/**
 * @brief 获取网络端口信息；
 * @return 返回整型端口号；
 */
int HttpConn::GetPort() const { // 改成unsigned int应该会更好
    return addr_.sin_port;
}

/**
 * @brief 从套接字描述符中读取数据；
 * @return 成功读取的长度；
 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno); // 读取缓冲区中接收到的请求报文；
        if (len <= 0) { // 读取失败
            break;
        }
    } while (isET); // 边缘触发就是一直读，因为边缘触发仅在被监视的文件描述符发生变化时才会触发事件通知；
    return len;
}

/**
 * @brief 向套接字描述符中写入数据；
 * @return 返回成功写入的字节数；
 */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;

    // 下面这个循环保证了在每一轮的读取中，会不断更新iov_数组中两个缓冲区的地址与长度信息；
    // 读完之后两个缓冲区应该都空了；
    do {
        len = writev(fd_, iov_, iovCnt_);   // 将缓冲区内容写入套接字描述符的字节数，iov_本身是指针(第三个参数应该是传入的结构体数组的长度)
        if(len <= 0) {  // len等于0是不是不一定就是写入失败了呢，所以这部分逻辑需要细细想想，或者说saveErrno是否会自动处理？
            *saveErrno = errno;
            break;
        }

        // 下面这段写入的内容会不会超过两个缓冲区呢？答案是不会的，因为上面返回的len就是从这2个缓冲区读取而来的
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; }  // 缓冲区没数据了，传输结束
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {   // 转化一下类型(ssize_t->size_t)，如果需要写入的内容长度大于第一个缓冲区之长
            // uint8_t的作用是在这个场景下提供了一种精确的、字节级别的指针操作方式。
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);   // 第二个缓冲区的指针向后移动
            iov_[1].iov_len -= (len - iov_[0].iov_len); // 更新第二个缓冲区的长度
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();   // 缓冲区与iov_不应该是独立的吗(是独立的)
                iov_[0].iov_len = 0;    // 第一个缓冲区的长度设置为0(读完了)
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;    // 更新起始位置
            iov_[0].iov_len -= len;     // 更新长度
            writeBuff_.Retrieve(len);   // 清空这部分内容，这部分数据已经写入了描述符，写入的这些数据准备---->客户端；
        }
    } while(isET || ToWriteBytes() > 10240);    // 如果是边缘触发，同样不断读取，10240的字节大小是根据网络负载设定的(每一轮都会更新结构体缓冲大小)
    return len;     // 返回最后一次循环中len的值有什么意义
}

/**
 * @brief 这是连接最核心的处理流程，接收客户端的请求报文，然后设置好缓冲区；
 */
bool HttpConn::process() {  // 该函数还没将缓冲区信息写入到套接字描述符，可以预见的是，必然是要先process，再write；
    request_.Init();    // 初始化http请求报文类
    if(readBuff_.ReadableBytes() <= 0) {    // 缓冲区中没有可读的数
        return false;
    }
    else if(request_.parse(readBuff_)) {    // 服务器会将请求报文读入到缓冲区并解析；
        LOG_DEBUG("%s", request_.path().c_str());   // 路径信息打印；
        // 下面这行代码，http回应http请求，持久连接与否同request保持一致，200表示成功
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);   // 解析成功则返回响应
    } else {
        response_.Init(srcDir, request_.path(), false, 400);    // 解析失败则返回错误信息，错误码设置为400
    }

    // 解析完http的请求消息之后，服务器返回请求报文；
    response_.MakeResponse(writeBuff_); // 服务器将响应报文写入到写缓冲区；
    // 数组中第一个缓冲区写入响应报文的信息；
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;    // 缓冲区数量；

    // 再向客户端发送HTML文件的内容，该内容传输新增一个缓冲区，提高传输效率；
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
