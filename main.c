#include "camera.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> 
#include<pthread.h>
#include <linux/videodev2.h>
#include <string.h>  // 对于 memset
#include <sys/ioctl.h>  
#include "camera_client.h"


pthread_attr_t attr;
pthread_t tid;
extern int v4l2_fd;
pthread_mutex_t mutex;

extern int save_flag; 

/*
另一个线程的运行函数，一直获取输入缓冲区的值，如果值为s，会将flag置为1，然后让v4l2_read保存图片

*/
 static void * save_picture(void * arg)
{
    char a;
	int fd =(int)arg;
	struct v4l2_queryctrl qctrl;
	struct v4l2_control ctl;
	int delta;
	memset(&qctrl,0,sizeof(qctrl));
	qctrl.id=V4L2_CID_BRIGHTNESS; //查询摄像头的亮度参数信息
	
	if(0!=ioctl(fd,VIDIOC_QUERYCTRL,&qctrl))
		{
		printf("亮度信息获取失败\n");
		return NULL;

		}
	printf("亮度幅度是 min=%d, max=%d\n",qctrl.minimum,qctrl.maximum);
	delta=(qctrl.maximum-qctrl.minimum)/10;
	ctl.id=V4L2_CID_BRIGHTNESS;
	ioctl(fd,VIDIOC_G_CTRL,&ctl);
    while(1){
    a=getchar();
    pthread_mutex_lock(&mutex); //互斥锁上锁
    if(a=='s')
    save_flag=1;
  	else if(a=='u')
	ctl.value+=delta;
	else if(a=='d')
		ctl.value-=delta;
	if(ctl.value>qctrl.maximum)
		ctl.value=qctrl.maximum;
	if(ctl.value<qctrl.minimum)
		ctl.value=qctrl.minimum;

    pthread_mutex_unlock(&mutex);//互斥锁解锁
    ioctl(fd,VIDIOC_S_CTRL,&ctl);
    	}
}


int main(int argc,char **argv)
{


     int  ret;
    if (2 != argc) {
        fprintf(stderr, "Usage: %s <video_dev>\n", argv[0]);
        exit(EXIT_FAILURE);
     }
   if (fb_dev_init())
        exit(EXIT_FAILURE);
	camera_prepare_init(argv); //一键初始化所有的摄像头功能

	 pthread_mutex_init(&mutex, NULL);
	 pthread_attr_init(&attr);
camera_client_init();
 /* 设置以分离状态启动线程 */
 pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
 ret=pthread_create(&tid,&attr,save_picture,(void *)v4l2_fd);

 v4l2_read(); //在函数内循环采集数据、将其显示到 LCD 屏
 pthread_attr_destroy(&attr);
 camera_client_end();
 exit(EXIT_SUCCESS);
}
