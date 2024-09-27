#include  <sys/socket.h>
#include  <sys/types.h>
#include   <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>


static int count=0;
static char filename[12];
#define handle_error(cmd,result)\
	if(result<0)				\
	{							\
	perror(cmd);				\
	return -1;					\
	}							\

static int flag=0;
static void * read_pic(void * arg)
{

 	int client_fd=(int)arg;
	int recv_count=0;
	snprintf(filename, sizeof(filename), "./%d%s", count, ".jpeg");
	FILE * fd_save = fopen(filename, "w+"); //在当前电脑创建一个文件
	char *read_buf=NULL;
	read_buf=malloc(sizeof(char)*1024);
	if(!read_buf)
	{
	perror("malloc server read buf");
	}
	//如果发送者已经调用 shutdown 来结束传输，或者网络协议支持按默认的顺序关闭并且发送端已经关闭，
//那么当所有的数据接收完毕后，recv 会返回 0。
//recv 在调用成功情况下返回实际读取到的字节数。
	while(recv_count=recv(client_fd,read_buf,1024,0))
	{
		printf("recv count =%d\n",recv_count);

	if(recv_count<0)
		{perror("recv");}
	 if (recv_count >= 3 && memcmp(read_buf + recv_count - 3, "EOF", 3) == 0) {
        break;  // 退出循环
   		 }

	printf("recv count =%d\n",recv_count);
	fwrite(read_buf,sizeof(char),recv_count,fd_save);
	}
	count++;
	if(recv_count==0)
	flag=1;
	printf("客户端关闭\n");
	free(read_buf);
	fclose(fd_save);
}
int main(void)
{
	
	int socketfd,client_fd;
	int temp_result;
	pthread_t pid_read;
	
	struct sockaddr_in server_addr,client_addr;
	memset(&server_addr,0,sizeof(server_addr));
	memset(&client_addr,0,sizeof(client_addr)); 
	server_addr.sin_family=AF_INET;  //属于ipv4家族
	inet_pton(AF_INET,"192.168.10.100",&server_addr.sin_addr);
//	server_addr.sin_addr.s_addr=htonl(INADDR_ANY); //服务端的ip地址随便填
	server_addr.sin_port=htons(666);


	socketfd=socket(AF_INET,SOCK_STREAM,0);//创建socket套接字
	handle_error("socket",socketfd);


	temp_result=bind(socketfd, (struct sockaddr *)&server_addr,sizeof(server_addr));  //绑定地址
	handle_error("bind",temp_result);


	temp_result=listen(socketfd,128); //进入监听状态
	handle_error("listen",temp_result); 

	socklen_t clientaddr_len=sizeof(client_addr);
			client_fd=accept(socketfd, (struct sockaddr *)&client_addr,&clientaddr_len);
	//如果调用accept后没有客户端连接，这里会挂起等待	
	printf("已连接到客户端 客户端 ip是 %s \n",inet_ntoa(client_addr.sin_addr));
	while(1&&flag==0)
	{
	//设置flag的原因，你客户端取消连接，pthread——create还是会继续执行，并且接收不到recv直接结束线程，会一直重复，所以我在接受数据的时候加了一个判断，就是recv=0的时候证明接收不到数据，客户端断开连接，服务端也就结束了
	//之所以不能用shutdown会直接关闭客户端连接，我要是拍摄多个图片还要重复进行连接
	pthread_create(&pid_read,NULL,read_pic,(void *) client_fd);
	pthread_join(pid_read,NULL);

	}
	close(socketfd);
	close(client_fd);
	

}

