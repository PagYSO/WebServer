#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc,char* argv[]){
	if(argc<=2){//Q
		printf("Usage: %s ip_address portname\n",argv[0]);
		return 0;
	}
	const char* ip=argv[1];
	int port=atoi(argv[2]);//Q
	
	int lfd=socket(PF_INET,SOCK_STREAM,0);
	assert(lfd>=1);

	struct sockaddr_in serv_addr;
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(port);
	inet_pton(AF_INET,ip,&serv_addr.sin_addr);

	int ret=0;
	ret=bind(lfd,(struct sockaddr *)(&serv_addr),sizeof(serv_addr));
	assert(ret!=-1);

	ret=listen(lfd,5);
	assert(ret!=-1);

	struct sockaddr_in clit_addr;
	socklen_t clit_addr_len=sizeof(clit_addr);

	int cfd=accept(cfd,(struct sockaddr *)&clit_addr,&clit_addr_len);

	char buf_size[1024]={0};
	int recv_size=recv(cfd,buf_size,sizeof(buf_size),0);
	int send_size=send(cfd,buf_size,recv_size,0);

	close(cfd);
	close(lfd);

	return 0;
}
