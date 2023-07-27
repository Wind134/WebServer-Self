/*
HTTP响应的头文件，封装了http响应环节的诸多方法
 */ 
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // 主要用于文件描述符
#include <unistd.h>      // 访问系统调用
#include <sys/stat.h>    // 访问文件状态
#include <sys/mman.h>    // 内存映射相关

#include "../data_buffer/buffer.h"
#include "../log_system/log.h"

class HttpResponse {
public:
    HttpResponse();     // 构造函数
    ~HttpResponse();    // 析构函数

    // 初始化http连接响应
    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    
    // 建立http响应
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
    // 添加状态行，信息写入内存缓冲区
    void AddStateLine_(Buffer &buff);

    // 添加Header内容进缓冲区，即连接信息写入缓冲区
    void AddHeader_(Buffer &buff);

    // 将网页内容放入缓冲区
    void AddContent_(Buffer &buff);

    // 将错误码与展示的网页路径对应起来
    void ErrorHtml_();
    // 获取文件类型
    std::string GetFileType_();

    int code_;              // 定义的应该是错误码
    bool isKeepAlive_;      // 连接类型

    std::string path_;      // 路径
    std::string srcDir_;    // 表示资源或者源文件的目录
    
    char* mmFile_;      // 指向内存映射的字符串内容
    struct stat mmFileStat_;    // 这是保存文件元数据的结构体

    // static变量声明，将文件后缀与文件类型相互对应的映射
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;

    // 错误类型->错误内容的映射
    static const std::unordered_map<int, std::string> CODE_STATUS;

    // 错误类型->网页展示路径的映射
    static const std::unordered_map<int, std::string> CODE_PATH;
};


#endif //HTTP_RESPONSE_H
