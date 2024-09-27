#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include "lcd.h"
#include"camera.h"
#include<pthread.h>
#include "camera_client.h"
#include  <sys/socket.h>
#include <poll.h>
//write by lzy 2024.8.30  email:lbw66606@163.com



extern int socketfd; 
#define JPEG_EXT ".jpeg"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) // 读写权限
static char * buffer;
const char *end_marker = "EOF";  // 或者使用其他你选择的结束标志

#define uint16_t unsigned short 
#define uint8_t unsigned char 
#define RGB565(r, g, b) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) //宏定义 取出RGB565 的r g b三个分量
static int count_file=1; //记录保存了多少张图片

extern pthread_mutex_t mutex;
 int v4l2_fd=-1; //打开的摄像头文件的fd （文件描述符）
int width, height; //LCD屏幕的宽度和高度
static int frm_width,frm_height; //实际的宽度和高度
static cam_fmt cam_fmts[10];//记录摄像头支持的像素格式
 //申请的缓冲帧个数
extern unsigned short * screen_base;   //LCD屏幕的地址
cam_buf_info buf_infos[frame_buffer_count];//保存映射好的地址 
extern unsigned long screen_size;  //屏幕的数据大小字节数
int save_flag=0;   //保存图片的标志位

#define argb8888_to_rgb565(color) ({ \
 unsigned int temp = (color); \
 ((temp & 0xF80000UL) >> 8) | \
 ((temp & 0xFC00UL) >> 5) | \
 ((temp & 0xF8UL) >> 3); \
 })





/*
函数功能：打开摄像头文件，并且查询摄像头是否具备捕获能力
*/
static int camera_init(const char * device)
{
    struct v4l2_capability cap={0};
 
	v4l2_fd=open(device,O_RDWR); //打开摄像头节点
	if(v4l2_fd<0)
	{
	    perror("open camera error");
	    return -1;
	}
	
	ioctl(v4l2_fd,VIDIOC_QUERYCAP,&cap); //查询摄像头是否具备捕获能力
	if(!(V4L2_CAP_VIDEO_CAPTURE&cap.capabilities))
	{
	    fprintf(stderr,"Error %s :no capture video device \n",device);
	    close(v4l2_fd);
	    return -1;
	}


	return 0;
}

/*
函数功能：枚举出摄像头所支持的所有像素格式并且记录在cam_fmts里面
此函数之前必须调用camera_init确保摄像头是捕获设备
*/
static void v4l2_enum_formats(void)
{

	struct v4l2_fmtdesc fmtdesc={0};
	/* 枚举出摄像头所支持的所有像素格式以及描述信息 */

	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/*
	description 字段是一个简单地描述性字符串，简单描述 pixelformat 像素格式。
	pixelformat 字段则是对应的像素格式编号，这是一个无符号 32 位数据，每一种像素格式都会使用一个u32 类型数据来表示
*/
	while(0==ioctl(v4l2_fd,VIDIOC_ENUM_FMT,&fmtdesc))
	{
		   cam_fmts[fmtdesc.index].pixelformat=fmtdesc.pixelformat;
		   strcpy(cam_fmts[fmtdesc.index].description,fmtdesc.description);
		 fmtdesc.index++;
	}


}


/*
函数功能：打印摄像头支持的所有视频分辨率和采集帧率
此函数之前必须调用enmu_formats,否则cam_fmts是0 也就是说for循环不能进行

*/

static void v4l2_print_formats(void) 
{
        struct v4l2_frmsizeenum frmsize = {0};   //枚举摄像头所支持的所有视频采集分辨率：
        struct v4l2_frmivalenum frmival = {0};  //枚举摄像头所支持的所有视频采集帧率：
        int i;
        
        frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            for(i=0;cam_fmts[i].pixelformat;i++)
            {
                printf("format<0x%x>,descriotion<%s>\n",cam_fmts[i].pixelformat,cam_fmts[i].description); 
                frmsize.index = 0;
                frmsize.pixel_format = cam_fmts[i].pixelformat;
                frmival.pixel_format = cam_fmts[i].pixelformat; //当前像素格式
				//先获取分辨率，再根据获取的分辨率查询当前分辨率下的所有帧率
                        while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {
                                        printf("size<%d*%d> ",frmsize.discrete.width,frmsize.discrete.height);
                                        frmsize.index++; //枚举下一个分辨率
                                        frmival.index = 0;
                                        frmival.width = frmsize.discrete.width; //当前分辨率的宽度
                                        frmival.height = frmsize.discrete.height; //当前分辨率的宽度
                                                            while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
                                                            printf("<%dfps>", frmival.discrete.denominator/frmival.discrete.numerator);
                                                            frmival.index++;
                                                            }
                                                            printf("\n");
            }
 printf("\n");
}
}


/*
函数功能：设置摄像头的模式，并且记录实际的帧宽度和高度，设置帧率
*/
static int v4l2_set_format(void)
{
	 struct v4l2_format fmt = {0};
	 struct v4l2_streamparm streamparm = {0};
	 fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//type 类型
	 fmt.fmt.pix.width = width; //视频帧宽度
	 fmt.fmt.pix.height = height;//视频帧高度
	 fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565; //像素格式

	 
	 if (0 > ioctl(v4l2_fd, VIDIOC_S_FMT, &fmt)) 
	 {
		 fprintf(stderr, "ioctl error: VIDIOC_S_FMT: %s\n", strerror(errno));
		 return -1;
	 }
	 /*** 判断是否已经设置为我们要求的 RGB565 像素格式
	 如果没有设置成功表示该设备不支持 RGB565 像素格式 */
	 
	 if (V4L2_PIX_FMT_RGB565 != fmt.fmt.pix.pixelformat) {
	 fprintf(stderr, "Error: the device does not support RGB565 format!\n");

	 }
	 frm_width = fmt.fmt.pix.width; //获取实际的帧宽度
	 frm_height = fmt.fmt.pix.height;//获取实际的帧高度
	 printf("视频帧大小<%d * %d>\n", frm_width, frm_height);
	 /* 获取 streamparm   这里获取流类型相关参数：比如是否支持帧率设置 是否高性能拍摄*/
	 streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	 ioctl(v4l2_fd, VIDIOC_G_PARM, &streamparm);
	 if (V4L2_CAP_TIMEPERFRAME & streamparm.parm.capture.capability) {
		 streamparm.parm.capture.timeperframe.numerator = 1;
		 streamparm.parm.capture.timeperframe.denominator = 30;//30fps
		 //设置为30帧 因为上一步并没有设置帧率
	 if (0 > ioctl(v4l2_fd, VIDIOC_S_PARM, &streamparm)) {
		 fprintf(stderr, "ioctl error: VIDIOC_S_PARM: %s\n", strerror(errno));
		 return -1;
	 }
	 }
	 return 0;
}

/*
streaming I/O 方式会在内核空间中维护一个帧缓冲队列，驱动程序会将从摄像头读取的一帧数据写入到
队列中的一个帧缓冲，接着将下一帧数据写入到队列中的下一个帧缓冲；当应用程序需要读取一帧数据时，
需要从队列中取出一个装满一帧数据的帧缓冲，这个取出过程就叫做出队；当应用程序处理完这一帧数据
后，需要再把这个帧缓冲加入到内核的帧缓冲队列中，这个过程叫做入队！这个很容易理解，现实当中都有
很多这样的例子，这里就不再举例了*/
//帧缓冲队列在内核空间
/*
使用 VIDIOC_REQBUFS 指令申请帧缓冲，该缓冲区实质上是由内核所维护的，应用程序不能直接读
取该缓冲区的数据，我们需要将其映射到用户空间中，这样，应用程序读取映射区的数据实际上就是读取内
核维护的帧缓冲中的数据。*/

static int v4l2_init_buffer(void)
{
	struct v4l2_requestbuffers reqbuf = {0};
	struct v4l2_buffer buf = {0};
	 reqbuf.count = frame_buffer_count; //帧缓冲的数量
	 reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	 reqbuf.memory = V4L2_MEMORY_MMAP;
	 if (0 > ioctl(v4l2_fd, VIDIOC_REQBUFS, &reqbuf)) {
	 fprintf(stderr, "ioctl error: VIDIOC_REQBUFS: %s\n", strerror(errno));
	 return -1;
	 }
	 //申请成功后，将内核中的帧映射到进程
	 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	 buf.memory = V4L2_MEMORY_MMAP;
	 for (buf.index = 0; buf.index < frame_buffer_count; buf.index++)
	 	{
			 ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf);//这个的作用只是查询每一块帧缓冲的大小和偏移量
			 //至于偏移量是什么 
			 /*
				因为应用程序通过 VIDIOC_REQBUFS 指令申请帧缓冲时，内核会向操作系统申请一块内存空间作为帧缓冲
				区，这块内存空间的大小就等于申请的帧缓冲数量 * 每一个帧缓冲的大小，每一个帧缓冲对应到这一块内
				存空间的某一段，所以它们都有一个地址偏移量。
			 */
			 buf_infos[buf.index].length = buf.length;
			 buf_infos[buf.index].start = mmap(NULL, buf.length,
			 PROT_READ | PROT_WRITE, MAP_SHARED,
			 v4l2_fd, buf.m.offset);
			 //mmap 为啥是v4l2_fd 估计是内核申请的帧缓冲保存在v4l2fd的文件里面
			 if (MAP_FAILED == buf_infos[buf.index].start) 
			 		{
				 perror("mmap error");
				 return -1;
			 		}
		 }
	 //使用 VIDIOC_QBUF 指令将帧缓冲放入到内核的帧缓冲队列中
	
	 for (buf.index = 0; buf.index < frame_buffer_count; buf.index++) {
	 if (0 > ioctl(v4l2_fd, VIDIOC_QBUF, &buf)) {
	 fprintf(stderr, "ioctl error: VIDIOC_QBUF: %s\n", strerror(errno));
	 return -1;
	 }
	 }
 return 0;
}

/*
函数功能：打开摄像头
*/
static int v4l2_stream_on(void)
{
 /* 打开摄像头、摄像头开始采集数据 */
 enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 if (0 > ioctl(v4l2_fd, VIDIOC_STREAMON, &type)) {
 fprintf(stderr, "ioctl error: VIDIOC_STREAMON: %s\n", strerror(errno));
 return -1;
 }
  printf("摄像头开启成功\n"); 
 return 0;
}



/*
按顺序初始化摄像头并且将所有的参数设置好
*/
void  camera_prepare_init(char ** argv)
{

	/* 初始化摄像头 */
	if (camera_init(argv[1]))
	exit(EXIT_FAILURE);
	/* 枚举所有格式并打印摄像头支持的分辨率及帧率 */
	v4l2_enum_formats();
	v4l2_print_formats();
	/* 设置格式 */

	if (v4l2_set_format())
	exit(EXIT_FAILURE);

	if (v4l2_init_buffer())
	exit(EXIT_FAILURE);

	/* 开启视频采集 */
	if (v4l2_stream_on())
	exit(EXIT_FAILURE);

}


 int CLIP(int x) {
	 if (x < 0) return 0;
	 if (x > 255) return 255;
	 return x;
 }
 
 
 
 void yuv_rgb(uint16_t *source, uint16_t *rgb) {
	 // 提取YUV分量
	 uint8_t y0 = (uint8_t)(source[0] & 0xFF); // 第一个Y分量
	 uint8_t u = (uint8_t)(source[0] >> 8);  // U分量
	 uint8_t y1 = (uint8_t)(source[1] & 0xFF); // 第二个Y分量
	 uint8_t v = (uint8_t)(source[1] >> 8);  // V分量
 
	 // 调整YUV分量的范围
	 int adjusted_v = v - 18; // 增加或减少一些值来调整颜色
 
	 // 转换第一个YUV到RGB
	 int r = CLIP((298 * (y0 - 16) + 409 * (adjusted_v - 128) + 128) >> 8);
	 int g = CLIP((298 * (y0 - 16) - 100 * (u - 128) - 208 * (adjusted_v - 128) + 128) >> 8);
	 int b = CLIP((298 * (y0 - 16) + 516 * (u - 128) + 128) >> 8);
 
	 // 存储第一个RGB565值
	 *rgb = RGB565(r, g, b);
 
	 // 转换第二个YUV到RGB
	 r = CLIP((298 * (y1 - 16) + 409 * (adjusted_v - 128) + 128) >> 8);
	 g = CLIP((298 * (y1 - 16) - 100 * (u - 128) - 208 * (adjusted_v - 128) + 128) >> 8);
	 b = CLIP((298 * (y1 - 16) + 516 * (u - 128) + 128) >> 8);
 
	 // 存储第二个RGB565值
	 *(rgb + 1) = RGB565(r, g, b);
 }



 void v4l2_read(void)
{
	 char filename[12];
	 int bytes_read;
	 unsigned int out_size=0;
	 struct v4l2_buffer buf ={0};
	 unsigned short  *base;
	 unsigned short * start;
	 int min_w,min_h;
	 if (width > frm_width)
	 min_w = frm_width;
	 else
	 min_w = width;
	 if (height > frm_height)
	 min_h = frm_height;
	 else
	 min_h = height;
	 //确定实际的buffer长和宽
	 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	 buf.memory = V4L2_MEMORY_MMAP;
	 struct pollfd fds;
	 fds.events=POLLIN;
	 fds.fd=v4l2_fd;
	 fds.revents=0;
	while(1){
		//if(1==poll(&fds,1,-1)) //没有数据处于阻塞态，有数据就进行下一步
		for(buf.index=0;buf.index<frame_buffer_count;buf.index++)
		{
 			
        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf); //出队 并且可以知道index 哪个帧有数据 bytesused 有多少数据
		base=screen_base;
		start=buf_infos[buf.index].start;
	
        if(save_flag) //如果saveflag触发会将文件保存
		{
		 	pthread_mutex_lock(&mutex); //互斥锁上锁
		  	save_flag=0;
		      	
		   	pthread_mutex_unlock(&mutex); //互斥锁上锁
	
				
		        snprintf(filename, sizeof(filename), "./%d%s", count_file, JPEG_EXT);

		        // 使用 open 创建文件，O_CREAT | O_WRONLY | O_TRUNC 表示创建文件，若文件存在则覆盖
		        FILE * fd = fopen(filename, "w+");
				
		        // 检查文件描述符是否有效
		        if (fd < 0) {
		            perror("Error creating file");
		            return ;
		        }
				out_size= yuv420sp_to_jpg(frm_width,frm_height,(unsigned char *) start,fd);
				fclose(fd);
				fd = fopen(filename, "r");  //因为上面我是以只写创建的，我要读取只能重新开一个fd
				buffer=malloc(sizeof(char)*1024);
				 while ((bytes_read = fread(buffer, sizeof(char), sizeof(buffer), fd)) > 0) //不断读取数据通过socket发送出去
				 	{
				 
					if (send(socketfd, buffer, bytes_read, 0) < 0) {
						 perror("send failed");
						 fclose(fd);
						 close(socketfd);
						 exit(EXIT_FAILURE);
					 }
				 }

				if (send(socketfd, end_marker, strlen(end_marker), 0) < 0) {  //就是发送完以后要告诉客户端这张照片已经结束，我就发送一个字符串EOF，让客户端接收到EOF做相应的处理
				    perror("send end marker failed");
				    fclose(fd);
				    close(socketfd);
				    exit(EXIT_FAILURE);
				}
				free(buffer); //释放空间

				 fclose(fd);

	
				printf("outsize=%d\n",out_size);

		        printf("camera ok%d\n",count_file);
		      
		        count_file++;
		   
		       
		
		     
        }
        else
        {
    

        for(int i=0;i<min_h;i++)
        {
        
           for(int l=0;l<min_w;l=l+2)
              yuv_rgb(start+l,base+l);
      
            
            start+=frm_width; //图片的实际宽度
			base+=width;     //LCD的显示宽度
        }
   
		
  
	
    	}



		ioctl(v4l2_fd, VIDIOC_QBUF, &buf);



	
    }
}
 	}

