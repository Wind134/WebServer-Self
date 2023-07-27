/*
连接模块的具体实现，其中处理请求与相应的方法即为process方法
*/
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;   // 资源路径
std::atomic<int> HttpConn::userCount;   // 用户数量，原子变量，操作它的时候不能干扰
bool HttpConn::isET;

HttpConn::HttpConn() {  // 初始化文件描述符，地址信息，连接状态
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;    // 更新用户数量
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();   // 清空缓冲区所有字符
    readBuff_.RetrieveAll();    // 清空缓冲区所有字符
    isClose_ = false;
    // 把当作客户端？目前没有太理解
    // fd是服务端为客户端生成的用于处理数据I/O的描述符，现在懂了
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();  // 首先解除内存中文件内容的映射
    if(isClose_ == false){  // 如果是连接着的状态
        isClose_ = true;    // 更新状态
        userCount--;        // 用户状态减一
        close(fd_);         // 关闭文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {   // 返回文件描述符
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {  // 返回地址信息
    return addr_;
}

const char* HttpConn::GetIP() const {   // 该函数将一个IPv4转换为一个点分十进制字符串
    return inet_ntoa(addr_.sin_addr);   
}

int HttpConn::GetPort() const { // 获取端口
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno); // 返回从文件中读取进缓冲区的字节数(注意，这里相对缓冲区而言就是写入了)
        if (len <= 0) { // 读取失败
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {   // uint8_t的作用是在这个场景下提供了一种精确的、字节级别的指针操作方式。
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);   // 成功将缓冲区内容写入文件的字节数，iov_本身是指针(第三个参数应该是传入的结构体数组的长度)
        if(len <= 0) {  // len等于0是不是不一定就是写入失败了呢，所以这部分逻辑需要细细想想
            *saveErrno = errno;
            break;
        }

        // 下面这段写入的内容会不会超过两个缓冲区呢？答案是不会的，因为上面返回的len就是从这2个缓冲区读取而来的
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; }  // 缓冲区没数据了，传输结束
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {   // 转化一下类型(ssize_t->size_t)，如果需要写入的内容长度大于第一个缓冲区之长
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);   // 第二个缓冲区的指针向后移动
            iov_[1].iov_len -= (len - iov_[0].iov_len); // 更新第二个缓冲区的长度
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();   // 缓冲区与iov_不应该是独立的吗(是独立的)
                iov_[0].iov_len = 0;    // 第一个缓冲区的长度设置为0
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;    // 更新起始位置
            iov_[0].iov_len -= len;     // 更新长度
            writeBuff_.Retrieve(len);   // 清空这部分内容
        }
    } while(isET || ToWriteBytes() > 10240);    // 如果是边缘触发，则不断读取，10240的字节大小是根据网络负载设定的(每一轮都会更新结构体缓冲大小)
    return len;     // 返回最后一次循环中len的值有什么意义
}

bool HttpConn::process() {
    request_.Init();    // 初始化http连接请求
    if(readBuff_.ReadableBytes() <= 0) {    // 缓冲区中没有可读的数
        return false;
    }
    else if(request_.parse(readBuff_)) {    // 信息传入读缓冲区
        LOG_DEBUG("%s", request_.path().c_str());
        // 下面这行代码，http回应http请求，持久连接与否同request保持一致，200表示成功
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), false, 400);    // 客户端请求有错误，设置为400
    }

    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
