/*
- HTTP响应报文处理的实现；
- 响应报文是服务器发给客户端的；
- 而请求报文是客户端发给服务器的；
- 这里用到了文件映射机制，即将文件中的内容全盘映射到内存；
*/ 
#include "httpresponse.h"
using namespace std;

/**
 * @brief 文件后缀对应的文件类型的映射，文件类型的数据信息来源于请求报文的请求头部；
 */
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

/**
 * @brief 状态码-->服务器状态的映射；
*/
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

/**
 * @brief 状态码-->展示给用户的界面网页路径的映射；
*/
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

/**
 * @brief 这是一个构造函数，初始化http相应处理中的一系列变量；
 */
HttpResponse::HttpResponse() {
    code_ = -1;             // 错误码默认定义为-1                
    path_ = srcDir_ = "";   // 路径
    isKeepAlive_ = false;   // 默认的http类型，是非持久类型
    mmFile_ = nullptr;      // 初始化指向映射的字符串内容的指针
    mmFileStat_ = { 0 };    // 结构体的初始化语法，状态初始化为0
};

/**
 * @brief 析构函数的工作就是解除文件映射；
 */
HttpResponse::~HttpResponse() {
    UnmapFile();
}

/**
 * @brief 发送响应报文阶段的初始化操作；
 * @param srcDir 资源文件目录的地址；
 * @param path 资源文件的地址(以srcDir为根目录)；
 * @param isKeepAlive 是否持久连接；
 * @param code 状态码；
 */
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");       // 既然是初始化，那么要求srcDir不为空
    if(mmFile_) { UnmapFile(); }// 如果指向的内容不为空(有数据)，那么先将指向的内容释放
    code_ = code;
    isKeepAlive_ = isKeepAlive; // 跟着初始化设定的参数走
    path_ = path;               
    srcDir_ = srcDir;
    mmFile_ = nullptr;          // 初始化
    mmFileStat_ = { 0 };
}

/**
 * @brief 向客户端发送响应报文；
 * @param buff 向缓冲区写入响应报文，buff是写入的目标缓冲区；
 */
void HttpResponse::MakeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    // string的data函数返回一个底层字符串指针，这段字符串会传进指向mmFileStat_变量的地址
    // 如果stat的返回值小于0，那么表明获取失败，或者获取到的文件信息是一个目录，那么返回404(没找到)
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) { // 如果其他用户没有(读)访问权限，则错误码设定为403
        code_ = 403;
    }
    else if(code_ == -1) { // code还是初始设定的-1，表明没有发生任何错误，那就将状态码设定为200
        code_ = 200; 
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

/**
 * @brief 获取文件在内存中的地址；
 * @return 文件内容的内存首地址；
 */
char* HttpResponse::File() {
    return mmFile_;
}

/**
 * @brief 获取文件的大小；
 * @return 文件的大小；
 */
size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size; // 返回文件大小，默认单位是字节
}

/**
 * @brief 该函数的功能是将状态码与要展示的HTML文件路径对应起来；
 */
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {   // 如果三个错误码中存在某一个
        path_ = CODE_PATH.find(code_)->second;  // 获取路径
        stat((srcDir_ + path_).data(), &mmFileStat_);   // 将路径写入到mmFileStat_指向的结构体中
    }
}

/**
 * @brief 将HTTP的响应报文中的状态行添加到服务器的缓冲区；
 * @param buff 服务器缓冲区的引用；
 */
void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;  // 错误码标识的具体状态
    if(CODE_STATUS.count(code_) == 1) { // 只要返回了code_信息，则标识状态
        status = CODE_STATUS.find(code_)->second;
    }
    else {  // 如果没有返回，表明request部分有问题
        code_ = 400;    // 此时更新错误码为400，表明"Bad Request!"
        status = CODE_STATUS.find(400)->second; // 更新状态
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");    // 将这段内容加入缓冲区(包括换行)
}

/**
 * @brief 将响应头部内容添加到缓冲区；
 * @param buff 服务器缓冲区的引用；
 */
void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {  // 如果是持久连接
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");  // 持久连接的单个连接的最大请求数量是6，120s内没有发出新的请求则关闭
    } else{ // 如果不是持久连接，则写入close信息
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

/**
 * @brief 将网页文件的内容写入到缓冲区(这里只添加了响应头部字段中的Content-length属性)；
 */
void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);   // 以只读方式打开网页文件
    if(srcFd < 0) { // 文件打开失败，则向内容中写入具体的错误信息；
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    // 将文件映射到内存提高文件的访问速度 
    // MAP_PRIVATE 建立一个写入时拷贝的私有映射
    // 也就是对内存内容的修改不会影响文本本身的内容
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());    // 在日志上打印网页文件的具体路径信息
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");   // 如果有错误信息，那么将错误信息写入到响应体，没有错误信息，响应体不写入信息；
        return; 
    }
    mmFile_ = (char*)mmRet; // 将内容映射到了内存，并转为字符串格式
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");  // 响应头与响应体之间有一空行；
}

/**
 * @brief 解除文件在内存中的映射；
 */
void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);   // 解除文件映射的函数，其中第二个参数是文件大小
        mmFile_ = nullptr;  // 相应指针置空
    }
}

/**
 * @brief 根据后缀来判断文件类型；
 * @return 返回具体的文件类型；
 */
string HttpResponse::GetFileType_() {
    /* 判断文件类型 */
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) {   // 对于没有后缀的文件，默认是文本
        return "text/plain";
    }
    string suffix = path_.substr(idx);  // 获取后缀
    if(SUFFIX_TYPE.count(suffix) == 1) {    // 如果在map有相应后缀，返回相应类型
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";    // 否则默认还是返回纯文本格式
}

/**
 * @brief 将错误内容写入到缓冲区，倒不是直接写入错误信息，而是会加入一些HTML标签；
 * @param message 具体的错误信息；
 */
void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    // 如果所料不差，那么下面展示的应该就是平时遇到的那一大串无情的空白页面在左上角附带一个冰冷的文字；
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {     // 如果是4字段的状态码，则更新其状态信息
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request"; // 如果是其他状态则更新为Bad Request
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>WebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");  // 细节，在添加响应体的时候给了一个空格字符；
    buff.Append(body);
}
