/*
缓冲区的具体实现，后续的http response要用到缓冲区的内容
*/ 
#include "buffer.h"

/**
 * @brief 这是buffer的构造函数，构造函数将读位置和写位置都进行初始化，同时定义了缓冲区大小，缓冲区内存储的信息全是字符类型；
 * @param initBuffSize 初始化的缓冲区大小，默认为1024字节
*/
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}


/**
 * @brief 获取缓冲区内可读字节数的函数；
 * @return 写位置和读位置的差，获得了剩余的可读字节数
*/
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

/**
 * @brief 获取缓冲区内还可写的字节数；
 * @return 写位置和读位置的差，也可以理解为缓冲区内目前剩余的空间；
*/
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

/**
 * @brief 可重利用的空间大小；
 * @return 读取到的位置
*/
size_t Buffer::PrependableBytes() const 
{
    return readPos_;    // 读位置之前的数据都已经读了，现在这部分空间可以预留给新的数据
}

/**
 * @brief 获取读位置在物理内存中的实际地址
 * @return 缓冲区的起始内存地址加上读位置的偏移量 
*/
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

/**
 * @brief 恢复len长度的空间，执行这一步必须要确保len字节的数据确实已读；
 * @param len 恢复的空间长度
 */
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes()); // 如果长度大于可读，显然就没有读完，是有问题的
    readPos_ += len;
}

/**
 * @brief 恢复直到指定内存地址的空间；
 * @param end 字符的内存地址，表明内存中该字符之前的所有字符已读；
 */
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

/**
 * @brief 清空缓冲区的所有内容，重新初始化读位置和写位置；
 */
void Buffer::RetrieveAll() {
    memset(&buffer_[0], 0, buffer_.size()); // 将buffer_清零，这部分的代码我作了更改
    readPos_ = 0;
    writePos_ = 0;
}

/**
 * @brief 先将缓冲区内还未读取的数据交给一个字符串，然后清空缓冲区；
 * @return 先前的字符串信息；
 */
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

/**
 * @brief 常量版本，获取写入到的位置的真实内存地址；
 */
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

/**
 * @brief 非常量版本，获取写入到的位置的真实内存地址；
 */
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

/**
 * @brief 确保数据已经写入到了缓冲区之后更新写入位置；
 * @param len 写入的数据大小；
*/
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

/**
 * @brief 添加内容到缓冲区；
 * @param str 要添加的字符串地址，由于str的参数是可以确定的，因此只需要一个参数；
 */
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

/**
 * @brief 重载的函数，添加内容到缓冲区；
 * @param data 要添加内容，任意类型的常量；
 * @param len 字符串的长度，该长度应该与上面的内容紧密联系；
 */
void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);    // 任意类型的数据都转化为字符串形式(强制类型转换)
}

/**
 * @brief 添加内容到该缓冲区，如果长度不够，再增加本缓冲区的长度，保证大小足够以容纳新的信息；
 * @param str 要添加的字符串地址；
 * @param len 字符串的长度，该长度应该与上面的地址紧密联系；
 */
void Buffer::Append(const char* str, size_t len) {
    assert(str);    // 确保指针不为空
    EnsureWriteable(len);   // 确保空间足够写入
    std::copy(str, str + len, BeginWrite());    // 将str的指定长度数据复制进以写入位置为起始位置的内存空间
    HasWritten(len);    // 已经写入了len长度的数据
}

/**
 * @brief 添加内容到缓冲区；
 * @param buff 对另一个缓冲区对象的引用；
 */
void Buffer::Append(const Buffer& buff) 
{
    // 这里不考虑是本字符串的情况，主要是没有重载等于运算符
    Append(buff.Peek(), buff.ReadableBytes());
}

/**
 * @brief 该函数确保缓冲区大小充足，如果不充足则会启用重新分配机制；
 * @param len 需求的字节大小；
*/
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

/**
 * @brief 读取文件中的信息到缓冲区；
 * @param fd 文件描述符信息；
 * @param saveErrno 指向保存错误码的地址；
 * @return 成功读取到缓冲区的字节数；
 */
ssize_t Buffer::ReadFd(int fd, int* saveErrno) { 
    char buff[65535];   // 这是一个临时缓冲区，也用来保存字符信息，也用来准备后续的添加操作；

    // struct iovec 包含两个成员变量：
    // void* iov_base：指向内存块的起始地址。
    // size_t iov_len：内存块的长度。
    // iov数组当然是连续的，但是数组中的元素是一个结构体，而其中结构体中的指针指向的
    // 内存是不连续的，这一点要理解
    struct iovec iov[2];    // 分别存储当前对象的缓冲区和buf缓冲区
    const size_t writable = WritableBytes();    // 缓冲区当前可写入的空间大小

    /* 分散读(内存空间不连续)， 保证数据全部读完 */
    // 首先是第一个缓冲区，就是我们的缓冲区对象
    iov[0].iov_base = BeginPtr_() + writePos_;  // 写入的起始地址
    iov[0].iov_len = writable;
    // 下面是第二个缓冲区
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    // readv函数用于从文件描述符中读取多个非连续的内存块的数据
    // 就是说readv可以从文件描述符中读取数据到多个缓冲区(非连续内存块)
    // 返回的是文件描述符实际读取的字节数
    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) *saveErrno = errno; // 读取失败则保存错误码到saveErrno中
    else if(static_cast<size_t>(len) <= writable)   writePos_ += len;   // 更新读取位置
    else    // 如果超过了该缓冲区对象的长度
    {
        writePos_ = buffer_.size();     // 长度大于可读取的范围，说明缓冲区已满
        Append(buff, len - writable);   // 剩余的数据追加到本缓冲区，如果空间不足，缓冲区会自动增长空间以满足需求
    }
    return len; // 确保都处理好了之后，就返回
}

/**
 * @brief 将换缓冲区中的内容写入到文件中，换句话说就是将内容写入到文件中，然后更新缓冲区的读位置
 * @param fd 要保存到的文件描述符；
 * @param saveErrno 指向保存错误码的地址；
*/
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

/**
 * @brief 获取缓冲区的起始内存地址，这个函数很好的展示了迭代器与指针的区别
 * @return 首元素的地址值  
*/
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();   // (*buffer_.begin())返回的是buffer_首元素的引用，取的是首元素地址
}

/** 
 * @brief 重载版本，获取缓冲区的真实内存地址；
*/
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

/**
 * @brief 函数保证缓冲区有足够的空间来容纳指定长度为len的数据，该函数实现了缓冲区的自动增长；
 * @param len 要分配的字节数
*/
void Buffer::MakeSpace_(size_t len) 
{
    if(WritableBytes() + PrependableBytes() < len)  // 已读取的空间(可重新利用)与可写入数据的空间加起来的长度不够len，说明不够用
        buffer_.resize(writePos_ + len + 1);        // 需要重新分配空间，加1是为了给空字符'\0'留一份空间
    else
    {
        size_t readable = ReadableBytes();

        // 把剩余未读取的内容重新放到开头
        // 不考虑使用移动构造，移动构造本身处理char类型的字符效率也没好到哪里去
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;   // 更新读位置，处理读位置的这一步不会被打断
        writePos_ = readPos_ + readable;    // 更新写位置(这里有一个细节，即便readPos_更新为0了，还是用它来加，是否有用意)
        // 如果这边触发了断言，那很有可能是出现了问题，也许是多线程操作导致的
        assert(readable == ReadableBytes());    // 这个数据按理说不会变，但是由于多线程的原因，可能会导致意外的更改
    }
}