#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include<mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAll.h"

class HttpRequest{
public:
    //状态机流程
    //如果为GET请求： REQUEST_LINE——>HEADERS——>FINISH；
    //如果为POST请求：REQUEST_LINE——>HEADERS——>BODY——>FINISH。
    enum PARSE_STATE{
        REQUEST_LINE,//正在解析请求首行
        HEADERS,//头
        BODY,//体
        FINISH,//完成
    };

    /* 服务器处理HTTP请求的结果：NO_REQUEST表示请求不完整，需要继续读取客户数据；GET_REQUEST表示获得了一个完整的客户请求；
    * BAD_REQUEST表示客户请求有语法错误；FORBIDDEN_REQUEST表示客户对资源没有足够的访问权限；INTERNAL_ERROR表示服务器内部错误；
    * CLOSED_CONNECTION表示客户端已经关闭连接了 
    */
    enum HTTP_CODE{
        NO_REQUESET=0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERAL_ERROR,
        CLOSED_CONNECTION,
    };
    HttpRequest() {init();}
    ~HttpRequest()=default;

    void init();
    bool parse(Buffer& buff);//解析全过程

    std::string path() const;
    //表示提交请求页面完整地址的字符串，不包括域名
    std::string& path();
    //表示提交请求使用的HTTP方法。 它总是大写的(GET,POST)
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:
    bool ParseRequestLine_(const std::string& line);//解析状态行
    void ParseHeader_(const std::string& line);//解析头部
    void ParseBody_(const std::string& line);//解析请求体

    void ParsePath_();//解析请求路径
    void ParsePost_();//解析Post请求
    void ParseFromUrlencoded_();//解析Post请求的请求参数

    //身份验证
    static bool UserVerify(const std::string& name,const std::string& pwd,bool isLogin);

    PARSE_STATE state_;
    std::string method_,path_,version_,body_;
    std::unordered_map<std::string,std::string> header_;
    std::unordered_map<std::string,std::string> post_;

    ////存储请求头字段
    static const std::unordered_set<std::string> DEFAULT_HTML;
    //存储POST请求参数
    static const std::unordered_map<std::string,int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};


#endif
