/*
缓冲区的具体实现，后续的http response要用到缓冲区的内容
 */ 
#include "buffer.h"

// 缓冲区的构造函数，默认分配可以容纳1024个的char类型元素的vector
// 写入位置为0，读取位置为0
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 还能读取的字符数：
// 写入位置和读取位置是在不断向前进的，对于读而言，接下来还能读多少，要用写
// 入到的位置减去已读取到位置；
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 还能写的字符数(是字符数)
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 预留的能新写入的字符数；
size_t Buffer::PrependableBytes() const 
{
    /* 
    * 读位置之前的数据都已经读了，现在这部分空间可以预留给
    * 新的数据，缓冲区的作用就是开辟一块内存空间，这片空间
    * 内可以同时进行读写
    */
    return readPos_;
}

// 获取当前读位置的内存实际地址(指针)
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 通过参数更新已读取的位置
// 表明len长度的数据已经读过了
// 读过的这部分说明已经写入文件了
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

// 给定内存地址end，将读取位置跳转到给定位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

// 清空缓冲区所有字符
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size()); // 将buffer_清零
    readPos_ = 0;
    writePos_ = 0;
}

// 将缓冲区中剩余数据拷贝到一个新的str对象中
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 获取写入位置处的内存地址，常量版本
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

// 非常量版本
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

// 通过参数更新写入位置，这里的len是已经写入的长度
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

// 以下几个Append函数都是重载函数
// 参数基本都是const版本，因为我们只是为了在缓冲区添加元素，而无需改动；
// 参数为string类型时
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

// 参数为指向data数据的指针，且提供要添加的数据长度时
void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);    // 任意类型的数据都转化为字符串形式(强制类型转换)
}

// 参数为字符指针类型时，且提供字符长
void Buffer::Append(const char* str, size_t len) {
    assert(str);    // 确保指针不为空
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());    // 将str的指定长度数据复制进以写入位置为起始位置的内存空间
    HasWritten(len);    // 已经写入了len长度的数据
}

// 参数为另一缓冲区对象的引用时
// 将另一缓冲区对象(未读取)的数据追加到当前缓冲区中，妙啊
void Buffer::Append(const Buffer& buff) 
{
    // 是不是可以考虑该对象是自己本身的情况
    Append(buff.Peek(), buff.ReadableBytes());
}

// 确保可写入，如果空间不够，则新分配空间；
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

// 该函数想要实现的功能是成功读入文件中的内容到缓冲区
// 换句话说就是读取文件内容之后，写入到缓冲区
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    
    char buff[65535];   // 这是一个临时缓冲区

    // struct iovec 包含两个成员变量：
    // void* iov_base：指向内存块的起始地址。
    // size_t iov_len：内存块的长度。
    // iov数组当然是连续的，但是数组中的元素是一个结构体，而其中结构体中的指针指向的
    // 内存是不连续的，这一点要理解
    struct iovec iov[2];    // 分别存储当前对象的缓冲区和buf缓冲区
    const size_t writable = WritableBytes();    // 当前还可写入的空间大小

    /* 分散读(内存空间不连续)， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;  // 写入的起始地址
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    // readv函数用于从文件描述符中读取多个非连续的内存块的数据
    // 就是说readv可以从文件描述符中读取数据到多个缓冲区(非连续内存块)
    // 返回的是文件描述符实际读取的字节数
    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) *saveErrno = errno; // 读取失败则保存错误码到saveErrno中
    else if(static_cast<size_t>(len) <= writable)   writePos_ += len;   // 更新读取位置
    else 
    {
        writePos_ = buffer_.size();     // 长度大于可读取的范围，说明缓冲区已满
        Append(buff, len - writable);   // 剩余的数据追加到本缓冲区末尾
    }
    return len;
}

// 该函数想要实现的功能是将换缓冲区中的内容写入到文件中
// 换句话说就是读取缓冲区后，将内容写入到文件中
ssize_t Buffer::WriteFd(int fd, int* saveErrno)
{
    size_t readSize = ReadableBytes();  // 还能读取的长度

    // write返回成功写入的字节数
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    }
    // 表明这部分内容成功写入文件，可以更新读位置了
    readPos_ += len;
    return len; // 也是返回写入的字节数
}

// 字符串起始位置的内存地址
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();   // *buffer_.begin()返回的是buffer_首元素的引用，取的是首元素地址
}

// const版本
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

// 这个函数的目的是保证缓冲区有足够的空间来容纳指定长度len的数据
void Buffer::MakeSpace_(size_t len) 
{
    if(WritableBytes() + PrependableBytes() < len)  // 已读取的空间(可重新利用)与可写入数据的空间加起来的长度不够len
        buffer_.resize(writePos_ + len + 1);        // 重新分配空间，加1是为了考虑'\0'？
    else
    {
        size_t readable = ReadableBytes();

        // 把剩余未读取的内容重新放到开头
        // 后续改进可以考虑移动构造函数是吧
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;   // 更新读位置
        writePos_ = readPos_ + readable;    // 更新写位置
        assert(readable == ReadableBytes());    // 这个数据不会变吧....
    }
}