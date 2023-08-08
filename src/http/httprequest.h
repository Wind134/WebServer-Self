/*
功能：
- 处理http连接请求的类，将连接请求封装成一个单独的头文件；
- 实现了对http请求的处理；
 */ 
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>    // 正则功能
#include <errno.h>     
#include <mysql/mysql.h>  // mysql连接

#include "../data_buffer/buffer.h"          // 内存缓冲池
#include "../log_system/log.h"              // 日志处理
#include "../sql_connection_pool/sqlconnpool.h"    // 用户池
#include "../sql_connection_pool/sqlconnRAII.h"    // 数据库连接的RAII机制

class HttpRequest {
public:
    /**
     * @brief 这个枚举包括了HTTP的四种解析状态;
     */
    enum PARSE_STATE {
        REQUEST_LINE,   // HTTP报文的第一行是解析请求行
        HEADERS,        // HTTP报文的请求头部
        BODY,           // HTTP的请求体
        FINISH,         // 解析器的完成状态
    };

    /**
     * @brief 这个枚举包括了HTTP请求的返回状态，具体含义顾名思义；
    */
    enum HTTP_CODE {
        NO_REQUEST = 0, // 从0开始，这是枚举的功能
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    /**
     * @brief 构造函数初始化HTTP连接请求；
     */
    HttpRequest() { Init(); }

    ~HttpRequest() = default;

    void Init();

    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;  
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

    /* 
    计算实现对FormData以及Json的解析
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);
    void ParsePath_();
    void ParsePost_();
    
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_; // 定义一个枚举变量表示解析状态
    std::string method_, path_, version_, body_;    // 方法、(网页)路径、版本、请求体
    std::unordered_map<std::string, std::string> header_;   // 请求头部是键值对类型
    std::unordered_map<std::string, std::string> post_;     // POST方法中附带的请求体？

    static const std::unordered_set<std::string> DEFAULT_HTML;  // 存储的是默认路径
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // HTML->Tag的映射
    static int ConverHex(char ch);
};

#endif //HTTP_REQUEST_H
