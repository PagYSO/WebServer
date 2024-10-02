#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn(){
    fd_=-1;
    addr_={0};
    isClose_=true;
}

HttpConn::~HttpConn(){
    Close();
}

void HttpConn::init(int fd,const sockaddr_in& addr){
    assert(fd>0);
    UserCount++;
    addr_=addr;
    fd_=fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_=false;
    LOG_INFO("Client[%d][%d](%s,%d) in,userCount:%d",fd_,GetIP(),GetPort(),(int)userCount);
}

void HttpConn::Close(){
    response_.UnmapFile();
    if(isClose_==false){
        isClose_=true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(),GetPort(),(int)userCount);
    }
}

int HttpConn::GetFd() const{
    return fd_;
}

struct sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}

const char* HttpConn::GetIP() const{
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno){
    ssize_t len=-1;
    do{
        len=readBuff_.ReadFd(fd,saveErrno);
        if(len<=0) break;
    }while(isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno){
    ssize_t len=-1;
    do{
        len=writev(fd_,iov_,iovCnt_);
        if(len<=0){
            *saveErrno=errno;
            break;
        }
        if(iov_[0].iov_len+iov_[1].iov_len==0) break;
        else if(static_cast<size_t>(len)>iov_[0].iov_len){
            iov_[1].iov_base=(uint8_t*)iov_[1].iov_base+(len-iov_[0].iov_len);
            iov_[1].iov_len=(len-iov_[0].iov_len);
            if(iov_[0].iov_len){
                //如果第一个 iov 仍有数据长度（即已写入到 socket 中），则从写缓冲区中清空已写入的数据。
            //将第一个 iov 的长度设置为零，表示已完成写入。
                writeBuff_.RetrieveAll();
                iov_[0].iov_len=0;
            }
        }
        else{
            //如果写入的字节数在第一个 iov 的长度范围内，
            //更新它的 base 指针以及长度，将已写入的部分从缓冲区中移除。
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    }while(isET || ToWriteBytes()>10240);
    return len;
}

//处理 HTTP 请求，并生成相应的响应
bool HttpConn::process(){
    request_.Init();
    //检查读取缓冲区 readBuff_ 中的可读字节数。如果没有字节可读，返回 false，表示处理失败
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    //尝试解析读取缓冲区中的请求数据。如果请求解析成功，将请求路径记录到日志，
    //并初始化 response_ 对象，设定状态码为 200（表示成功）
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        //如果请求解析失败，初始化响应对象，设置状态码为 400（表示请求错误），
        //并关闭连接（IsKeepAlive 设置为 false）。
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    //将响应头的起始地址和长度设置到 iov_ 的第一个元素中，准备发送响应。
    //iovCnt_ 设置为 1，表示当前只有响应头需要发送
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    //如果响应包含文件（文件长度大于 0 且文件指针有效），
    //则将文件的地址和长度设置到 iov_ 的第二个元素中，并更新 iovCnt_ 为 2，表示还需要发送文件内容。
    //这里的 response_.File() 和 response_.FileLen() 可能用于获取要发送的文件数据及其大小。
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
