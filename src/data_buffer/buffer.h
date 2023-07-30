/*
对于缓冲区的处理，学习之前需要明确的信息是：
- read函数读取文件内容，然后这部分内容是写入缓冲区的，也就说针对缓冲区而言是写入行为，但是为了便于程序员使用我们仍然称为read函数
    -- 之所以提及到这部分，本质原因是因为，在套接字网络编程中，对于服务端而言，接受从客户端发来的数据就是读。读入到自己的缓冲区
    -- 客户端同理
- write函数将缓冲区内容写入文件，相对缓冲区而言是读取的过程 

缓冲区本身服务于服务端与客户端之间的数据交换(缓冲区的意义不用多说，OS里面的东西)
 */ 

#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>
#include <iostream>
#include <unistd.h>     // write
#include <sys/uio.h>    // readv
#include <vector>
#include <atomic>       // C++11标准中的一些表示线程、并发控制时原子操作的类与方法等
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;    // 默认析构

    size_t WritableBytes() const;
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;          // 字符串数组

    // 为什么要保证原子性呢，我的理解是，不同Buffer对象可对该位置进行改动
    // 为了防止某对象改动的时候，被其他需要改动的对象破坏了这个过程
    // 以至于该对象没有按照预期改动这个位置
    // 这是C++ 11标准以后的内容
    std::atomic<std::size_t> readPos_;  // 定义了读取到的位置，atomic保证原子性
    std::atomic<std::size_t> writePos_; // 定义了写入到的位置，保证原子性
};

#endif //BUFFER_H