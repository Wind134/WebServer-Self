/*
HTTP回应的头文件
 */ 
#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {   // 定义文件后缀->类型的映射
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

const unordered_map<int, string> HttpResponse::CODE_STATUS = {      // 映射错误码的具体内容
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {        // 错误类型与网页路径的映射
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;             // 错误码默认定义为-1                
    path_ = srcDir_ = "";   // 路径
    isKeepAlive_ = false;   // 默认的http类型，是非持久类型
    mmFile_ = nullptr;      // 初始化指向映射的字符串内容的指针
    mmFileStat_ = { 0 };    // 结构体的初始化语法，状态初始化为0
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");       // 既然是初始化，那么要求srcDir不为空
    if(mmFile_) { UnmapFile(); }// 如果指向的内容不为空，那么先将指向的内容释放
    code_ = code;
    isKeepAlive_ = isKeepAlive; // 跟着初始化设定的参数走
    path_ = path;               
    srcDir_ = srcDir;
    mmFile_ = nullptr;          // 初始化
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    // string的data函数返回一个底层字符串指针，这段字符串会传进指向mmFileStat_变量的地址
    // 如果获取失败，或者获取到的文件信息是一个目录，那么返回404
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) { // 如果其他用户没有访问权限，则错误码设定为403
        code_ = 403;
    }
    else if(code_ == -1) { // code还是-1，表明没有发生任何错误，那就将错误码设定为200
        code_ = 200; 
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size; // 返回文件大小，默认单位是字节
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {   // 如果三个错误码中存在某一个
        path_ = CODE_PATH.find(code_)->second;  // 获取路径
        stat((srcDir_ + path_).data(), &mmFileStat_);   // 将路径写入到mmFileStat_指向的结构体中
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;  // 错误码标识的具体状态
    if(CODE_STATUS.count(code_) == 1) { // 只要返回了code_信息，则标识状态
        status = CODE_STATUS.find(code_)->second;
    }
    else {  // 如果没有返回，表明request部分有问题
        code_ = 400;    // 此时更新错误码为400
        status = CODE_STATUS.find(400)->second; // 更新状态
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");    // 将这段内容加入缓冲区
}

void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {  // 如果是持久连接
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");  // 单个连接的最大请求数量是6，120s内没有发出新的请求则关闭
    } else{ // 如果不是持久连接，则写入close信息
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);   // 获取网页内容，写入缓冲区
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    // 将文件映射到内存提高文件的访问速度 
    // MAP_PRIVATE 建立一个写入时拷贝的私有映射
    // 也就是对内存内容的修改不会影响文本本身的内容
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = (char*)mmRet; // 将内容映射到了内存，并转为字符串格式
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);   // 解除文件映射的函数，其中第二个参数是文件大小
        mmFile_ = nullptr;  // 相应指针置空
    }
}

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

void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) { 
        status = CODE_STATUS.find(code_)->second;
    } else {    // http响应端无法接受请求
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
