
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
#include "jpeglib.h"
#include "lcd.h"
#include  <sys/socket.h>
#include  <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "camera_client.h"

#include "camera.h"

#define FD_DEV "/dev/fb0"

unsigned long screen_size;


#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) // 读写权限

unsigned short * screen_base=NULL;

extern int width ;
extern int height; 


static int fb_fd=-1; //屏幕的fd描述文件

/*
函数功能：对LCD屏幕进行初始化
Frame 是帧的意思，buffer 是缓冲的意思，所以 Framebuffer 就是帧缓冲，这意味着 Framebuffer 就是一
块内存，里面保存着一帧图像。帧缓冲（framebuffer）是 Linux 系统中的一种显示驱动接口，它将显示设备
（譬如 LCD）进行抽象、屏蔽了不同显示设备硬件的实现，对应用层抽象为一块显示内存（显存），它允
许上层应用程序直接对显示缓冲区进行读写操作，而用户不必关心物理显存的位置等具体细节，这些都由
Framebuffer 设备驱动来完成。

*/
 int fb_dev_init(void)
{
    struct fb_var_screeninfo fb_var={0}; //获取当前屏幕可变参数信息
    struct fb_fix_screeninfo fb_fix={0}; //获取当前设备的固定参数信息
   
    fb_fd=open(FD_DEV,O_RDWR);
    if(fb_fd<0)
    {
       perror("open error");
       return -1;
    }
    ioctl(fb_fd,FBIOGET_VSCREENINFO,&fb_var);
    ioctl(fb_fd,FBIOGET_FSCREENINFO,&fb_fix);
    screen_size=fb_fix.line_length*fb_var.yres;
    width=fb_var.xres;
    height=fb_var.yres;
    screen_base=mmap(NULL,screen_size,PROT_READ|PROT_WRITE,MAP_SHARED,fb_fd,0);
    if(MAP_FAILED==(void *)screen_base)
    {
        perror("mmap error");
        close(fb_fd);
    }
    memset(screen_base,0xFF,screen_size);  //直接对映射好的内存区域写值
    return 0;

}


/*
函数功能：对x进行限幅到0-255
*/

 unsigned int yuv420sp_to_jpg(int width, int height, unsigned char *inputYuv,FILE * fd)
 {
	 struct jpeg_compress_struct cinfo;
	 struct jpeg_error_mgr jerr;
	 JSAMPROW row_pointer[1];
	 int i = 0, j = 0;
	 unsigned char *pY0, *pU, *pV,*pY1;
	 unsigned char yuvbuf[width * 3];
	 unsigned long outSize;
	
	 cinfo.err = jpeg_std_error(&jerr);//用于错误信息
	 jpeg_create_compress(&cinfo);	//初始化压缩对象

	 cinfo.image_width = width;//设置输入图片宽度
	 cinfo.image_height = height;//设置图片高度
	 cinfo.input_components = 3;
	 cinfo.in_color_space = JCS_YCbCr;//设置输入图片的格式，支持RGB/YUV/YCC等等
	 cinfo.dct_method = JDCT_FLOAT;
	  jpeg_stdio_dest(&cinfo,fd);
	 jpeg_set_defaults(&cinfo);//其它参数设置为默认的！
	 jpeg_set_quality(&cinfo, 40, TRUE);//设置转化图片质量，范围0-100
	 jpeg_start_compress(&cinfo, TRUE);
	 pY0 = inputYuv ;
	 pU = inputYuv +1 ;
	 pY1=inputYuv+2;
	 pV = inputYuv + 3;
	 j = 1;
	 while (cinfo.next_scanline < cinfo.image_height) {
		 int index = 0;
		 for (i = 0; i < width; i += 2){//输入的YUV图片格式为标准的YUV444格式，所以需要把YUV420转化成YUV444.
			 yuvbuf[index++] = *pY0;
			 yuvbuf[index++] = *pU;
			 yuvbuf[index++] = *pV;
			 pY0 +=4;
			 yuvbuf[index++] = *pY1;
			 yuvbuf[index++] = *pU;
			 yuvbuf[index++] = *pV;
			 pY1 += 4;
			 pU += 4;
			 pV += 4;
		 }
		 row_pointer[0] = yuvbuf;
		 (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);//单行图片转换压缩
		 j++;
	 }
	 jpeg_finish_compress(&cinfo);
	 jpeg_destroy_compress(&cinfo);
	
	 return (unsigned int )outSize;
 }





