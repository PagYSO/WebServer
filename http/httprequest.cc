#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index","/register","/login",
    "/welcome","/video","/picture"};//这些路径可能是 Web 服务器处理请求时的默认选项
const unordered_map<string,int> HttpRequest::DEFAULT_HTML_TAG{
    {"register.html",0},{"/login.html",1},};

void HttpRequest::init(){
    method_=path_=version_=body_="";
    state_=REQUEST_LINE;
    header_.clear();
    post_.clear();
}

//检测 HTTP 请求是否应该保持连接
bool HttpRequest::IsKeepAlive() const{
    if(header_.count("Connection")==1){
        return header_.find("Connection")->second=="keep-alive"
            && version_=="1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff){
    const char CRLF[]="\r\n";//定义 HTTP 请求中行结束符的常量 CRLF，用于指示换行
    //定义 HTTP 请求中行结束符的常量 CRLF，用于指示换行
    if(buff.ReadableBytes()<=0){
        return false;
    }
    //在 Buffer 还有可读字节且解析状态不是 FINISH 的情况下，进入解析循环。
    while(buff.ReadableBytes() && state_!=FINISH){
        //使用 search 函数查找当前缓冲区中下一行的结束位置（即 CRLF）
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        //从 Buffer 中提取当前行，直到找到的 lineEnd 作为结束
        string line(buff.Peek(),lineEnd);
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)){
                return false;
            }
            ParsePath_();
            break;
        
        //解析请求头，如果在这之后没有足够的字节
        //（至少 2 个字节，包含 CRLF），则将状态设置为 FINISH
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes()<=2){
                state_=FINISH;
            }
            break;
        
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd==buff.BeginWrite()) break;
        buff.RetireveUntil(lineEnd+2);
    }
    //记录请求的 HTTP 方法、路径和版本
    LOG_DEBUG("[%s],[%s],[%s]",method_.c_str(),path_.c_str(),version_.c_str());
    return true;
}

//执行ParsePath_();解析路径资源,给path加上后缀名
void HttpRequest::ParsePath_(){
    if(path_=="/"){
        path_="/index.html";
    }
    else{
        for(auto &item:DEFAULT_HTML){
            if(item==path_){
                path_+=".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line){
    //匹配状态行
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    //方便的对匹配的结果进行获取。
    smatch subMatch;
    if(regex_match(line,subMatch,patten)){
        method_=subMatch[1];// 请求方法 
        path_=subMatch[2];//路径
        version_=subMatch[3];//版本
        state_=HEADERS;//设置状态为头部
        return true;
    }
    LOG_ERROR("Request Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line){
    //解析请求头部的字段
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    //line通过regex_match匹配，成功则存储至subMatch中
    if(regex_match(line,subMatch,patten)){
        header_[subMatch[1]]=subMatch[2];
    }
    else{
        state_=BODY;
    }
}

//值对应的结构便是请求结构头。将键和值存在header_这个表中。匹配失败时说明已经读完了请求头部，将当前的状态改为BODY
//如果当前缓存区可读的内容少于两个字节，说明没有请求体，将状态改为FINISH
void HttpRequest::ParseBody_(const string& line){
    body_=line;
    ParsePost_();
    state_=FINISH;
    LOG_DEBUG("Body:%s,len:%d",line.c_str(),line.size());
}

int HttpRequest::ConverHex(char ch){
    unsigned int y;
    if (ch >= 'A' && ch <= 'F') {
        y = ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
        y = ch - 'a' + 10;
    } else if (ch >= '0' && ch <= '9') {
        y = ch - '0';
    } else {
        assert(0);
    }
    return y;
}

//解析 HTTP POST 请求，并处理 URL 编码的表单数据
void HttpRequest::ParsePost_(){
    if(method_=="POST" && header_["Content-Type"]=="application/x-www-form-urlencoded"){
        //调用 ParseFromlencoded_() 函数解析 URL 编码的表单数据
        ParseFromUrlencoded_();
        //查找请求的路径 (path_) 是否存在相应的条目
        if(DEFAULT_HTML_TAG.count(path_)){
            int tag=DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d",tag);
            if(tag==0 || tag==1){
                bool isLogin=(tag==1);
                if(UserVerify(post_["UserName"],post_["password"],isLogin)){
                    path_="/welcome.html";
                }
                else{
                    path_="/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_(){
      if(body_.size()==0) return;
      string key,value;
      int num=0;
      int n=body_.size();
      int i=0,j=0;
      for(;i<n;i++){
        char ch=body_[i];
        switch(ch){
            //当遇到 = 字符时，表示键的结束，后续的部分为值的开始。
            case '=':
                key=body_.substr(j,i-j);
                j=j+1;
                break;
            //+ 字符会被替换为空格，这是 URL 编码的标准。
            case '+':
                body_[i]=' ';
                break;
            //% 字符后跟随两个十六进制数字，将其转换为对应的字符。
            case '%':
                num=ConverHex(body_[i+1]*16+ConverHex(body_[i+2]));
                body_[i+2]=num%10+'0';
                body_[i+1]=num/10+'0';
                n-=2;
                break;
            //& 字符表示一个键值对的结束，并将键值对存入 post_ 字典中
            case '&':
                value=body_.substr(j,i-j);
                j=i+1;
                post_[key]=value;
                LOG_DEBUG("%s=%s",key.c_str(),value.c_str());
                break;
            default:
                break;
        }
      }
      assert(j<=1);
      if(post_.count(key)==0 && j<i){
        value=body_.substr(j,i-j);
        post_[key]=value;
      }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
     if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
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

std::string HttpRequest::GetPost(const std::string& key) const{
    assert(key!="");
    if(post_.count(key)==1){
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const{
    assert(key!=nullptr);
    if(post_.count(key)==1){
        return post_.find(key)->second;
    }
    return "";
}


