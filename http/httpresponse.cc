#include "httpresponse.h"
using namespace std;

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

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse(){
    code_=-1;
    path_=src_="";
    isKeepAlive_=false;
    mmFile_=nullptr;
    mmFileStat_ = { 0 };
}

HttpResponse::~HttpResponse(){
    UnmapFile();
}

void HttpResponse::init(const string& src,string& path,bool isKeepAlive,int code){
    assert(src!="");
    if(mmFile_) UnmapFile();
    code_=code;
    isKeepAlive_=isKeepAlive;
    path_=path;
    src_=src;
    mmFile_=nullptr;
    mmFileStat_={0};
}

void HttpResponse::MakeResponse(Buffer& buff){
    //int stat(const char *pathname, struct stat *buf)
    //pathname：用于指定一个需要查看属性的文件路径。
    //buf：struct stat 类型指针，用于指向一个 struct stat 结构体变量。调用 stat 函数的时候需要传入一个 struct stat 变量的指针，获取到的文件属性信息就记录在 struct stat 结构体中。
    //S_ISDR判断一个路径是不是目录
    //st_mode:该字段用于描述文件的模式，譬如文件类型、文件权限都记录在该变量中
    if(stat((src_+path_).data(),&mmFileStat_)<0 || S_ISDIR(mmFileStat_.st_mode)){
        code_=404;
    }
    //表示用户具有读权限
    //判断判断文件所有者对该文件是否具有可读权限
    else if(!(mmFileStat_.st_mode & S_IROTH)){
        code_=403;
    }
    else if(code==-1){
        code_=200;
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File(){
    return mmFile_;
}

size_t HttpResponse::FileLen() const{
    return mmFileStat_.st_size();
}

void HttpResponse::ErrorContent(){
    if(CODE_PATH.count(code_)==1){
        path_=CODE_PATH.find(code_)->second;
        stat((src_+path_).data(),&mmFileStat_);
    }
}

HttpResponse::AddStateLine_(Buffer& buff){
    string status;
    if(CODE_STATUS.count(code_)==1){
        status=CODE_STATUS.find(code_)->second;
    }
    else{
        code_=400;
        status=CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1"+to_string(code_)+" "+status+"\r\n");
}

void HttpResponse::AddContent_(Buffer& buff){
    //O_RDONLY 只读
    int srcFd=open((src_+path_).data(),O_RDONLY));
    if(srcFd<0){
        ErrorContent(buff,"File NotFound");
        return;
    }

    LOG_DEBUG("file path %s".(src_+path_).data());
    //mmap分配一段匿名的虚拟内存区域，也可以映射一个文件到内存
    //期望的内存保护标志，不能与文件的打开模式冲突
    //PROT_EXEC //页内容可以被执行
    //PROT_READ //页内容可以被读取
    //PROT_WRITE //页可以被写入
    //PROT_NONE //页不可访问
    //指定映射对象的类型，映射选项和映射页是否可以共享。它的值可以是一个或者多个以下位的组合体
    // MAP_PRIVATE //建立一个写入时拷贝的私有映射。内存区域的写入不会影响到原文件。
    //这个标志和以上标志是互斥的，只能使用其中一个。
    int* mmRet=(int*)mmap(0,mmFileStat_.st_size(),PROT_READ,MAP_PRIVATE,srcFd,0);
    if(*mmRet==-1){
        ErrorContent(buff,"File NotFound");
        return;
    }
    mmFile_=(char*)mmRet;
    close(srcFd);
    buff.Append("Content=length:"+to_string(mmFileStat_.st_size));
}

void HttpResponse::UnmapFile(){
    if(mmFile_){
        //该调用在进程地址空间中解除一个映射关系
        munmap(mmFile_,mmFileStat_.st_size);
        mmFile_=nullptr;
    }
}

string HttpResponse::GetFileType_(){
    //判断文件类型
    //逆向查找在原字符串中最后一个与指定字符串（或字符）中的某个字符匹配的字符，返回它的位置。若查找失败，则返回npos。
    //（npos定义为保证大于任何有效下标的值
    string::size_type idx=path_.find_last_of('.');
    if(idx==string::npos){
        return "text/plain";
    }
    string suffix=path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix)==1){
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff,string message){
    string body;
    string status;
    body+="<html><title>Error</title>";
    body+="<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_)==1){
        status=CODE_STATUS.find(code_)->second;
    }
    else{
        status="Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
