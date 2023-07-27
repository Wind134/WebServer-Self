/*
头文件的实现
 */ 
#include "httprequest.h"
using namespace std;

// 下面展示的是一个默认路径，定义了六种路径
const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

// 展示了网页标签，注册是0，登录是1
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

// 将一个http请求初始化
void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";    // 四部分设为空
    state_ = REQUEST_LINE;  // 请求行状态，这是连接刚开始的状态
    header_.clear();        // 保证两个map均为空，header与post都是键值对
    post_.clear();
}

// 判断连接类型
// http的持久连接连接数只有1
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 连接请求针对字符串内容的解析
// 参数用了引用，表明可修改缓冲区内容
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n"; // 表示回车或者换行的字符串
    if(buff.ReadableBytes() <= 0) { // 缓冲区可读数据必须要存在
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {   // 只要有字符可读，且解析状态没有完整，则持续循环
        // 查找给定范围内范围内第一个CRLF子序列，HTTP的请求实质信息就是一串又一串字符串
        // 每部分内容通过CRLF字符做区分
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd); // 获取每一行的实质内容
        switch(state_)  // 根据state_的状态做处理
        {
        case REQUEST_LINE:  // 如果是解析请求行
            if(!ParseRequestLine_(line)) {  // 如果请求行解析失败，则返回错误
                return false;
            }
            ParsePath_();   // 解析成功则开始解析路径
            break;    
        case HEADERS:       // 如果是解析头部
            ParseHeader_(line); // 对头部解析，注意这边只有在到了最后的部分才会更改状态state
            if(buff.ReadableBytes() <= 2) { // 如果可读字节数<=2，表明到此已解析完成(最后两个字节只能是CRLF)
                state_ = FINISH;
            }
            break;
        case BODY:  // 如果是解析请求体
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; } // 到了尾部，直接返回
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") {  // 如果从根目录开始，则路径为/index(意思就是根目录'/'->'/index.html')
        path_ = "/index.html"; 
    }
    else {  // 如果不是，则看看路径是item中的哪个
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); // 通过正则表达式做匹配，patten是一个模式匹配对象，对象的规则如右
    smatch subMatch;    // subMatch的smatch匹配对象用于存储匹配结果的子匹配项
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];  // 第一个子匹配项是方法
        path_ = subMatch[2];    // 第二个子匹配项是路径
        version_ = subMatch[3]; // 第三个子匹配项是版本
        state_ = HEADERS;       // 将状态设置为头部，表示请求行已匹配完成，接下来解析头部
        return true;
    }
    LOG_ERROR("RequestLine Error"); // 如发生错误，将错误信息写入日志
    return false;
}

// 正则匹配的是：从开头开始，但不以':'字符开头的字符串
// 直到遇到':'后接可选的空格字符的情况，这种字符串可以匹配多个
// 每一个子匹配项就是括号内的内容
// Content-Type: application/json
// 针对每一行都会做匹配，如果符合头部的规则则做头部处理
// 如果不符合头部的规则那么处理的是请求体
void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   // 如果符合匹配规则，那对头部做键值处理
        header_[subMatch[1]] = subMatch[2];
    }
    else {  // 如果不符合这个规则，则说明要解析的是请求体
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;   // 头部解析完之后就是请求体
    ParsePost_();   // 调用Post解析函数
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    // 如果以application/x-www-form-urlencoded格式提交表单数据，则调用相应函数
    // 该格式数据展示：name=John+Doe&age=25&email=john.doe%40example.com
    // 空格字符被编码为+，而@符号被编码为%40
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i]; // 从上面的示例可以看出，当遇到等于号的时候，获取了第一个字段
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);   // 从0开始，长度(i - j)的字符做键
            j = i + 1;  // 更新j值
            break;
        case '+':
            body_[i] = ' '; // +号->空格
            break;
        case '%':
            // %后面接的是十六进制
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);   // 真实数字
            body_[i + 2] = num % 10 + '0';  // 都转成字符串
            body_[i + 1] = num / 10 + '0';
            i += 2; // 进2位
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value; // 更新键值对
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {    // 如果是没有匹配过的key，我们获取一个value，该value给这个key
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }   // 如果用户名为空或者密码为空，则直接返回错误的结果
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());   // 打印登录信息
    MYSQL* sql; // 定义一个指向sql连接的指针
    SqlConnRAII(&sql,  SqlConnPool::Instance());    // 通过RAII机制建立连接，同时确保顺利析构
    assert(sql);    // 确保sql存在
    
    bool flag = false;  // 这个标志位标记默认的验证结果
    // unsigned int j = 0; // 保存列数
    char order[256] = { 0 };    // 缓冲大小设置为256

    // 下面这段是使用MySQL提供的C语言的API进行数据库操作的一部分
    // MYSQL_FIELD结构用于描述查询结果集中的列信息
    // MYSQL_RES 结构用于保存查询结果集的信息
    // 在使用MySQL C API进行数据库查询时，通常会通过执行查询语句来获取查询结果集，
    // 然后通过mysql_store_result()函数将结果集存储在MYSQL_RES结构中。
    // 然后，可以使用mysql_fetch_field()函数获取结果集中的列信息，并将其存储在MYSQL_FIELD结构中。
    // MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }   // 如果并非登录，表明这是注册操作，注册操作先将flag设置为true
    // 通过给定的用户名进行查询，字符串的信息是一段SQL查询
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order); // 将order打印到日志

    if(mysql_query(sql, order)) {   // 执行查询，查询过程成功返回0，失败返回非0，与在不在结果中无关
        mysql_free_result(res);     // 释放已分配的内存，避免内存泄漏
        return false;               // 由于查询失败，用户核验失败，返回
    }

    // 能继续，说明查询到了信息
    res = mysql_store_result(sql);  // 从sql连接中查询所返回结果，并存储到res所指向的地址
    // j = mysql_num_fields(res);      // sql查询中列的数量
    // fields = mysql_fetch_fields(res);   // 从结果集中获取列(字段)信息

    // 将获取的行信息赋给row，只要行还有，循环就不停止
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);  // 用户名 密码
        string password(row[1]);
        /* 登录行为*/
        if(isLogin) {   // 如果是登录
            if(pwd == password) { flag = true; }    // 密码正确，flag标志为true
            else {
                flag = false;   // 信息核验失败
                LOG_DEBUG("pwd error!");    // 否则提示密码错误
            }
        } 
        else {  // isLogin为false则为假，表明是注册，但是该用户名又在数据库中(因为查到了)，因此提示用户用户名已使用
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res); // 释放内存

    /* 注册行为且用户名未被使用(这种情况下while循环应该不会执行) */
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");    // 打印注册信息
        bzero(order, 256);  // 给order赋0
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql, order)) {   // 判断插入是否成功，若不成功
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    // 注册完了就是登录上了
    SqlConnPool::Instance()->FreeConn(sql); // 将已连接的sql加入队列
    LOG_DEBUG( "UserVerify success!!");     // 表明用户核验成功
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}
