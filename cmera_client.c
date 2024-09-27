#include  <sys/socket.h>
#include  <sys/types.h>
#include   <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include "camera_client.h"
int socketfd;
	int temp_result;

#define handle_error(cmd,result)\
	if(result<0)				\
	{							\
	perror(cmd);				\
	return -1;					\
	}							\

	struct sockaddr_in server_addr,client_addr;

//初始化拍照传输客户端
int camera_client_init(void)
{
	


	pthread_t pid_read;
	

	memset(&server_addr,0,sizeof(server_addr));
	memset(&client_addr,0,sizeof(client_addr)); 
	server_addr.sin_family=AF_INET;  //属于ipv4家族
	inet_pton(AF_INET,"192.168.10.100",&server_addr.sin_addr);
//	server_addr.sin_addr.s_addr=htonl(INADDR_ANY); //服务端的ip地址随便填
	server_addr.sin_port=htons(666);

	client_addr.sin_family=AF_INET;
	inet_pton(AF_INET,"192.168.10.50",&client_addr.sin_addr);
	client_addr.sin_port=htons(8888);
	socketfd=socket(AF_INET,SOCK_STREAM,0);//创建socket套接字
	handle_error("socket",socketfd);


	temp_result=bind(socketfd,(struct sockaddr *)&client_addr,sizeof(server_addr));  //绑定地址
	handle_error("bind",temp_result);


	temp_result=connect(socketfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) ;//与客户端建立连接
	handle_error("connect",temp_result); 
}

void camera_client_end(void)
{
	close(socketfd);

}