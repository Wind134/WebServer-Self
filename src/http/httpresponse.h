/*
- 封装了处理HTTP响应报文的头文件；
- 封装了处理http响应报文环节的诸多方法；
- 涉及到的一些有用的知识：
    -- 在C/C++中，保存文件元数据的结构体 'struct stat' 用于存储有关文件的信息的数据结构，包括：
        --- st_mode：文件的类型和访问权限。
        --- st_size：文件的大小（以字节为单位）。
        --- st_atime：文件的最后访问时间。
        --- st_mtime：文件的最后修改时间。
        --- st_ctime：文件的最后状态更改时间。
    -- 内存映射机制，允许将一个文件或其他资源映射到进程的地址空间，使得可以通过指针对其进行读取和写入，就像访问内存一样。 
        --- 实现内存映射的函数：void *mmap()       
 */ 
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // 主要用于文件描述符
#include <unistd.h>      // 访问系统调用
#include <sys/stat.h>    // 访问文件状态
#include <sys/mman.h>    // 内存映射相关

#include "../data_buffer/buffer.h"  // 缓冲池
#include "../log_system/log.h"      // 日志系统

class HttpResponse {
public:
    HttpResponse();     // 构造函数
    ~HttpResponse();    // 析构函数

    
    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    
    void MakeResponse(Buffer& buff);
    
    // 该函数用来解除文件映射
    void UnmapFile();

    // 获取文件内容在内存中的地址，其实也就是获取文本
    char* File();
    // 获取文件长度
    size_t FileLen() const;

    // 将错误内容也写入缓冲区，message应该是传递更具体的内容，后续看运用
    void ErrorContent(Buffer& buff, std::string message);
    // 获取错误码
    int Code() const { return code_; }

private:

    void AddStateLine_(Buffer &buff);

    void AddHeader_(Buffer &buff);

    // 将网页内容放入缓冲区
    void AddContent_(Buffer &buff);

    void ErrorHtml_();
    
    std::string GetFileType_();

    int code_;              // 定义的应该是错误码
    bool isKeepAlive_;      // 连接类型

    std::string path_;      // 路径
    std::string srcDir_;    // 表示资源或者源文件的目录
    
    char* mmFile_;      // 指向内存映射的字符串内容
    struct stat mmFileStat_;    // 这是保存文件元数据的结构体

    // static变量声明，将文件后缀与文件类型相互对应的映射
    // 这里有一个C++的小知识点，静态成员变量不能再类内进行初始化；
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;

    static const std::unordered_map<int, std::string> CODE_STATUS;

    // 错误类型->网页展示路径的映射
    static const std::unordered_map<int, std::string> CODE_PATH;
};


#endif //HTTP_RESPONSE_H
