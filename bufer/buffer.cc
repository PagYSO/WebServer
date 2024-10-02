#include "buffer.h"

Buffer::Buffer(int initBufferSize):buffer_(initBufferSize),readPos_(0),writePos_(0){}

size_t Buffer::ReadableBytes() const{
    return writePos_-readPos_;
}

size_t Buffer::WritableBytes() const{
    return buffer_.size()-writePos_;
}

size_t Buffer::PrependableBytes() const{
    return readPos_;
}

//返回指向当前读位置的指针
const char* Buffer::Peek() const{
    return BeginPtr_()+readPos_;
}

//移动读位置len个字节
void Buffer::Retrieve(size_t len){
    assert(len<=ReadableBytes());
    readPos_+=len;
}

//移动读位置，直到达到end指向的位置
void Buffer::RetireveUntil(const char* end){
    assert(Peek()<=end);
    Retrieve(end-Peek());
}

//清空整个缓冲区，并将读位置和写位置重置为0
void Buffer::RetrieveAll(){
    bzero(&buffer_[0],buffer_.size());
    readPos_=0;
    writePos_=0;
}

//将当前可读的数据转换为字符串，并清空缓冲区
std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(),ReadableBytes());
    RetrieveAll();
    return str;
}

//返回指向当前写位置的只读指针
const char *Buffer::BeginWriteConst() const
{
    return BeginPtr_() + writePos_;
}

//返回指向当前写位置的可写指针
char *Buffer::BeginWrite()
{
    return BeginPtr_() + writePos_;
}

//移动写位置len个字节，表示写入了len个字节的数据
void Buffer::HasWritten(size_t len){
    writePos_+=len;
}

//将字符串str追加到缓冲区末尾
void Buffer::Append(const std::string& str){
    Append(str.data(),str.length());
}

//将数据data的len个字节追加到缓冲区末尾
void Buffer::Append(const void* data,size_t len){
    assert(data);
    //将void* 转化为*char*类型
    Append(static_cast<const char*>(data),len);
}

//将字符串str的len个字节追加到缓冲区末尾
void Buffer::Append(const char* str,size_t len){
    assert(str);
    EnsureWriteable(len);
    //copy只负责复制，不负责申请空间
    std::copy(str,str+len,BeginWrite());
    HasWritten(len);
}

//将另一个Buffer对象buff中的数据追加到当前缓冲区
void Buffer::Append(const Buffer& buff){
    Append(buff.Peek(),buff.ReadableBytes());
}

//确保缓冲区有足够的空间写入len个字节的数据
void Buffer::EnsureWriteable(size_t len){
    if(WritableBytes()<len){
        MakeSpace(len);
    }
    assert(WritableBytes()>=len);
}

ssize_t Buffer::ReadFd(int fd,int* saveErrno){
    char buff[65535];
    struct iovec iov[2];
    const size_t writeable=WritableBytes();

    //分散读，确保数据全部读完
    iov[0].iov_base=BeginPtr_()+writePos_;//iov_base 表示缓冲的地址
    iov[0].iov_len=writeable;//iov_len 表示缓冲的大小 
    iov[1].iov_base=buff;
    iov[1].iov_len=sizeof(buff);

    const ssize_t len=readv(fd,iov,2);
    if(len<0) *saveErrno=errno;
    else if(static_cast<size_t>(len)<=writeable) writePos_+=len;
    else{
        writePos_=buffer_.size();
        Append(buff,len-writeable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd,int* saveErrno){
    ssize_t readSize=ReadableBytes();
    ssize_t len=write(fd,Peek(),readSize);
    if(len<0){
        *saveErrno=errno;
        return len;
    }
    readPos_+=len;
    return len;
}

char* Buffer::BeginPtr_(){
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const{
    return &*buffer_.begin();
}

// 如果需要的空间大于当前可写空间和可前置空间的总和，重新分配缓冲区大小；
// 否则，将可读数据向前移动，为新数据腾出空间
void Buffer::MakeSpace(size_t len){
    if(WritableBytes()+PrependableBytes()<len){
        buffer_.resize(writePos_+len+1);
    }
    else{
        size_t readable=ReadableBytes();
        std::copy(BeginPtr_()+readPos_,BeginPtr_()+writePos_,BeginPtr_());
        readPos_=0;
        writePos_=readPos_+readable;
        assert(readable==ReadableBytes());
    }
}
