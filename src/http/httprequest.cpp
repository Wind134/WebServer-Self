/*
HTTP请求处理的实现
*/ 
#include "httprequest.h"
using namespace std;

/**
 * @brief 默认的HTML路径，注意，是HTML文件的(相对)路径；
 */
const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

/**
 * @brief 给路径加上了标签，注册的标签为0，登录的标签为1；
 */
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

/**
 * @brief 初始化一个http连接请求；
 */
void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";    // 方法为空，路径(URL)为空，HTTP版本为空，请求体默认也为空(不选)；
    state_ = REQUEST_LINE;  // 请求行(第一行)状态，这是连接刚开始的状态
    header_.clear();        // header是请求报文中的请求头部
    post_.clear();          // post应该是请求报文使用POST方法时附带的请求体
}

/**
 * @brief 判断HTTP请求是否是持久类型；
 */
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {  // 首先请求报文的请求头部要有Connection字段，其次版本要1.1，且是keep-alive
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

/**
 * @brief 针对传入缓冲区的请求报文字段做解析，也就是解析请求报文；
 * @param buff 缓冲区对象，引用类型，可修改；
 */
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";     // 表示回车或者换行的字符串(为了统一，选择用这个字符做换行)
    if(buff.ReadableBytes() <= 0) { // 缓冲区可读数据必须要存在
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {   // 只要有字符可读，且解析状态没有完整，则持续循环
        // 查找给定范围内范围内第一个CRLF子序列，HTTP的请求实质信息就是一串又一串字符串
        // 每部分内容通过CRLF字符做区分
        // 在可读取的地址范围中，寻找CRLF字符串信息，返回第一个匹配的位置(内存地址)
        const char* begin_read = buff.Peek();   // 起始读位置
        const char* end_read = buff.BeginWriteConst();  // 读位置的结束地址 
        const char* lineEnd = search(begin_read, end_read, CRLF, CRLF + 2);
        std::string line(begin_read, lineEnd); // 获取每一行的实质内容，首地址
        switch(state_)  // 根据state_的状态做处理
        {
        case REQUEST_LINE:  // 如果是解析请求行(第一行)
            if(!ParseRequestLine_(line)) {  // 如果请求行解析失败，则返回错误
                return false;
            }
            ParsePath_();   // 解析成功则开始解析路径
            break;    
        case HEADERS:       // 如果是解析头部
            ParseHeader_(line); // 对头部解析，注意这边只有在到了最后的部分才会更改状态state
            if(buff.ReadableBytes() <= 2) { // 如果可读字节数<=2，表明到此已解析完成(最后两个字节只能是CRLF)，说明不附带请求体
                state_ = FINISH;
            }
            break;
        case BODY:  // 如果是解析请求体
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) break; // 到了尾部，直接返回
        buff.RetrieveUntil(lineEnd + 2);    // 加上CRLF字符的两个字节
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

/**
 * @brief 解析请求行中的URL路径信息；
 */
void HttpRequest::ParsePath_() {
    if(path_ == "/") {  // 如果从根目录开始，则路径为/index(意思就是根目录'/'<==>'/index.html')
        path_ = "/index.html"; 
    }
    else {  // 如果不是，则看看路径是item中的哪个(假设一定会是其中某一个，还是说设定之初已表明他们会是其中某一个)
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

/**
 * @brief 解析请求行，并返回解析成功与否的结果；
 * @param line 请求行语句；
 * @return 解析的结果；
 */
bool HttpRequest::ParseRequestLine_(const string& line) {
    // 解析下面这段正则表达式：
    // ^表示匹配字符串的开始位置；
    // ([^ ]*)表示一个匹配组：[^ ]表示除空格以外的字符，*表示匹配任意个数这种字符，这里匹配出的结果应该是请求行的方法字段；
    // " "空格字符表明匹配空格，而后又去匹配那个匹配组，这个匹配组应该匹配的是路径字段；
    // " HTTP/"表明要准确匹配该字符，后面又是一个匹配组，这个匹配组匹配的是协议版本字段；
    // $表示匹配到字符串的结束位置；
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


/**
 * @brief 解析请求报文中的请求头部，但该函数没有返回值；
 * @param line 头部字段的字符串；
 */
void HttpRequest::ParseHeader_(const string& line) {
    // 解析下面这段正则表达式：
    // ^表示匹配字符串的开始位置；
    // ([^:]*)表示一个匹配组：[^:]表示除':'以外的字符，*表示匹配任意个数这种字符，这里匹配出的结果应该是map的键key；
    // ": "空格字符表明匹配': '，但后面的'?'让空格可有可无，即": "和':'都匹配；
    // 而后又去匹配匹配组(.*)，这个匹配组表明可以匹配任意字符；
    // $表示匹配到字符串的结束位置；
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   // 如果符合匹配规则，那对头部做键值处理
        header_[subMatch[1]] = subMatch[2];
    }
    else {  // 如果不符合这个规则，说明解析到了空行，到了空行说明请求报文中的请求头部已解析完，准备解析请求体；
        state_ = BODY;
    }
}

/**
 * @brief 解析请求体的内容；
 */
void HttpRequest::ParseBody_(const string& line) {
    body_ = line;   // 头部解析完之后就是请求体；
    ParsePost_();   // 调用Post解析函数，因为POST一般都会附带请求体；
    state_ = FINISH;// 更新状态；
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

/**
 * @brief 十六进制转十进制；
 * @return 返回十进制的值；
 */
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

/**
 * @brief 针对POST方法的请求报文中请求体的解析，这里会提交用户登录的表达信息；
 */
void HttpRequest::ParsePost_() {
    // 如果以application/x-www-form-urlencoded格式提交表单数据，则调用相应函数
    // 该格式数据展示：name=John+Doe&age=25&email=john.doe%40example.com
    // 空格字符被编码为+，而@符号被编码为%40
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) { // 如果路径在map中找到
            int tag = DEFAULT_HTML_TAG.find(path_)->second; // 获取标签
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";    // 展示欢迎页
                } 
                else {
                    path_ = "/error.html";      // 展示错误页
                }
            }
        }
    }   
}

/**
 * @brief 解析从url中编码而来的数据；
 */
void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }   // 这部分数据在请求体，如果没有请求体，显然不需要解析了

    string key, value;  // 键-值
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; ++i) {
        char ch = body_[i]; // 从上面的示例可以看出，当遇到等于号的时候，获取了第一个字段
        
        // 下面用switch base字段代替if的判断，本质上是一系列if
        // name=John%20Doe&age=25&city=New%20York 这就是一段编码的例子
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);   // 从0开始，长度(i - j)的字符做键
            j = i + 1;  // 更新j值，下一个开始索引段
            break;
        case '+':
            body_[i] = ' '; // +号->空格
            break;
        case '%':
            // %后面接的是十六进制，空格其实就是被编码为%20，但是+号一般在URL编码的数据中是用于编码空格的
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);   // 真实数字
            body_[i + 2] = num % 10 + '0';  // 都转成字符串
            body_[i + 1] = num / 10 + '0';
            i += 2; // 进2位
            break;
        case '&':   // 连接符'&'，表示的是[键，值]之间的连接
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
    if(post_.count(key) == 0 && j < i) {    // 如果是没有匹配过的key(没遇到'&')，我们获取一个value，该value给这个key
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

/**
 * @brief 实现用户信息核验功能；
 * @param name 账户信息；
 * @param pwd 密码信息；
 * @param isLogin 登录状态；
 * @return 用户信息核验结果；
 */
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }   // 如果用户名为空或者密码为空，则直接返回错误的结果
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());   // 打印登录信息
    MYSQL* sql; // 定义一个指向sql连接的指针，该sql可以获取到数据库连接池中的数据库连接，因此在定义数据库连接池的时候需要传指针的指针(引用也可)
    SqlConnRAII(&sql,  SqlConnPool::Instance());    // 通过RAII机制建立连接，同时确保顺利析构
    assert(sql);    // 确保sql存在
    
    bool flag = false;  // 这个标志位标记默认的验证结果
    // unsigned int j = 0; // 保存列数
    char order[256] = { 0 };    // 缓冲大小设置为256

    // 下面这段是使用MySQL提供的C语言的API进行数据库操作的一部分
    // MYSQL_FIELD结构用于描述查询结果集中的列信息
    // MYSQL_RES 结构用于保存查询结果集的信息
    // 在使用MySQL C API进行数据库查询时，通常会通过执行查询语句来获取查询结果集
    // 然后通过mysql_store_result()函数将结果集存储在MYSQL_RES结构中。
    // 然后，可以使用mysql_fetch_field()函数获取结果集中的列信息，并将其存储在MYSQL_FIELD结构中。
    // MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }   // 如果并非登录，表明这是注册操作，注册操作先将flag设置为true
    // 通过给定的用户名进行查询，字符串的信息是一段SQL查询
    // user已经锁定了表名，创建sql的语句中也用了user这个表名
    // 这条语句写入了order字符串，字符串有长度限制
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order); // 将保存的语句order打印到日志

    if(mysql_query(sql, order)) {   // 执行查询，查询过程成功返回0，失败返回非0，与在不在结果中无关
        mysql_free_result(res);     // 释放已分配的内存，避免内存泄漏
        return false;               // 由于查询失败，用户核验失败，返回
    }

    // 能继续，说明查询到了信息，sql是有效的
    res = mysql_store_result(sql);  // 从sql连接中查询所返回结果，并存储到res所指向的地址
    // j = mysql_num_fields(res);      // sql查询中列的数量
    // fields = mysql_fetch_fields(res);   // 从结果集中获取列(字段)信息

    // 将获取的行信息赋给row，只要行还有，循环就不停止(但这段逻辑，if是不是也可以)
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

    // 注册行为且用户名未被使用(这种情况下while循环应该不会执行，因为查不到)
    // 同时如果是登录行为，if这个逻辑不会执行
    // 如果不是登录行为，但flag为false，表明数据库中存在这么一个用户名，这段逻辑也不会执行
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");    // 打印注册信息
        bzero(order, 256);  // 给order赋0
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql, order)) {   // 在数据库中执行这段语句，并判断插入是否成功，若不成功返回相关信息；
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    // 注册完了就是登录上了
    SqlConnPool::Instance()->FreeConn(sql); // 将sql连接归还队列，也就是释放该连接；
    LOG_DEBUG( "UserVerify success!!");     // 表明用户核验成功
    return flag;
}

/**
 * @brief 获取html资源路径信息，返回常量版本；
 * @return 返回html路径；
 */
std::string HttpRequest::path() const{
    return path_;
}

/**
 * @brief 获取html资源路径信息，返回非常量版本；
 * @return 返回html路径；
 */
std::string& HttpRequest::path(){
    return path_;
}

/**
 * @brief 获取请求报文中的方法，返回常量版本；
 * @return 返回方法；
 */
std::string HttpRequest::method() const {
    return method_;
}

/**
 * @brief 获取HTTP的版本信息，返回常量版本；
 * @return 返回版本信息；
 */
std::string HttpRequest::version() const {
    return version_;
}

/**
 * @brief 获取请求体中键对应的值，返回常量版本；
 * @param key string类型的键属性；
 * @return 键属性对应值的值；
 */
std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

/**
 * @brief 获取请求体中键对应的值，返回常量版本；
 * @param key C版本字符串的键属性；
 * @return 键属性对应值的值；
 */
std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}
