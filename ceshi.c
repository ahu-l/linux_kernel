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
#include<pthread.h>
#include <jpeglib.h>
#define FD_DEV "/dev/fb0"
#define frame_buffer_count 3

#define JPEG_EXT ".jpeg"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) // 读写权限
#define RGB565(r, g, b) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
#define uint16_t unsigned short 
#define uint8_t unsigned char 
typedef struct camera_format
{
    unsigned char description[32];
    unsigned int pixelformat;
} cam_fmt;

typedef struct cam_buf_info{
unsigned short * start;
unsigned long length;
}cam_buf_info;
static int count_file=0;
static cam_buf_info buf_infos[frame_buffer_count];
static int fb_fd=-1;
static unsigned short * screen_base=NULL;
static int width;
static int height;
static int v4l2_fd=-1;
static pthread_mutex_t mutex;
static int height;
static cam_fmt cam_fmts[10];
static int frm_width,frm_height;
static int save_flag=0;
//******************************************************************//
static int fb_dev_init(void)
{
    struct fb_var_screeninfo fb_var={0};
    struct fb_fix_screeninfo fb_fix={0};
    unsigned long screen_size;
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
    memset(screen_base,0xFF,screen_size);
    return 0;

}

int CLIP(int x) {
    if (x < 0) return 0;
    if (x > 255) return 255;
    return x;
}
//************************************************************//
static int camera_init(const char * device)
{
    struct v4l2_capability cap={0};
    struct v4l2_fmtdesc fmtdesc={0};
v4l2_fd=open(device,O_RDWR);
if(v4l2_fd<0)
{
    perror("open camera error");
    return -1;
}
ioctl(v4l2_fd,VIDIOC_QUERYCAP,&cap);
if(!(V4L2_CAP_VIDEO_CAPTURE&cap.capabilities))
{
    fprintf(stderr,"Error %s :no capture video device \n",device);
    close(v4l2_fd);
    return -1;
}

fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
while(0==ioctl(v4l2_fd,VIDIOC_ENUM_FMT,&fmtdesc))
{
    cam_fmts[fmtdesc.index].pixelformat=fmtdesc.pixelformat;
    strcpy(cam_fmts[fmtdesc.index].description,fmtdesc.description);

fmtdesc.index++;
}
return 0;
}
// ***********************************************//
static void v4l2_print_formats(void)
{
    struct v4l2_frmsizeenum frmsize = {0};
 struct v4l2_frmivalenum frmival = {0};
 int i;
 frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 for(i=0;cam_fmts[i].pixelformat;i++)
 {
    printf("format<0x%x>,descriotion<%s>\n",cam_fmts[i].pixelformat,cam_fmts[i].description);
    frmsize.index = 0;
    frmsize.pixel_format = cam_fmts[i].pixelformat;
    frmival.pixel_format = cam_fmts[i].pixelformat;
            while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {
                            printf("size<%d*%d> ",
                            frmsize.discrete.width,
                            frmsize.discrete.height);
                            frmsize.index++;
                            /* 获取摄像头视频采集帧率 */
                            frmival.index = 0;
                            frmival.width = frmsize.discrete.width;
                            frmival.height = frmsize.discrete.height;
                                                while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
                                                printf("<%dfps>", frmival.discrete.denominator/frmival.discrete.numerator);
                                                frmival.index++;
                                                }
                                                printf("\n");
 }
 printf("\n");
}
}
//******************************************************//
static int v4l2_set_format(void)
{
    struct v4l2_format fmt = {0};
 struct v4l2_streamparm streamparm = {0};
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//type 类型
 fmt.fmt.pix.width = width; //视频帧宽度
 fmt.fmt.pix.height = height;//视频帧高度
 fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565; //像素格式
  if (0 > ioctl(v4l2_fd, VIDIOC_S_FMT, &fmt)) {
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
 /* 获取 streamparm */
 streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 ioctl(v4l2_fd, VIDIOC_G_PARM, &streamparm);
 if (V4L2_CAP_TIMEPERFRAME & streamparm.parm.capture.capability) {
 streamparm.parm.capture.timeperframe.numerator = 1;
 streamparm.parm.capture.timeperframe.denominator = 30;//30fps
 if (0 > ioctl(v4l2_fd, VIDIOC_S_PARM, &streamparm)) {
 fprintf(stderr, "ioctl error: VIDIOC_S_PARM: %s\n", strerror(errno));
 return -1;
 }
 }
 return 0;
}

/*****************************************/
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
 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 buf.memory = V4L2_MEMORY_MMAP;
 for (buf.index = 0; buf.index < frame_buffer_count; buf.index++) {
 ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf);
 buf_infos[buf.index].length = buf.length;
 buf_infos[buf.index].start = mmap(NULL, buf.length,
 PROT_READ | PROT_WRITE, MAP_SHARED,
 v4l2_fd, buf.m.offset);
 if (MAP_FAILED == buf_infos[buf.index].start) {
 perror("mmap error");
 return -1;
 }
 }

 for (buf.index = 0; buf.index < frame_buffer_count; buf.index++) {
 if (0 > ioctl(v4l2_fd, VIDIOC_QBUF, &buf)) {
 fprintf(stderr, "ioctl error: VIDIOC_QBUF: %s\n", strerror(errno));
 return -1;
 }
 }
 return 0;
}

static int v4l2_stream_on(void)
{
 /* 打开摄像头、摄像头开始采集数据 */
 enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 if (0 > ioctl(v4l2_fd, VIDIOC_STREAMON, &type)) {
 fprintf(stderr, "ioctl error: VIDIOC_STREAMON: %s\n", strerror(errno));
 return -1;
 }
 return 0;
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
unsigned int yuv420sp_to_jpg(int width, int height, unsigned char *inputYuv,unsigned char *outJpeg)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	int i = 0, j = 0;
	unsigned char *pY0, *pU, *pV,*pY1;
	unsigned char yuvbuf[width * 3];
	unsigned long outSize;
	cinfo.err = jpeg_std_error(&jerr);//用于错误信息
	jpeg_create_compress(&cinfo);  //初始化压缩对象
	jpeg_mem_dest(&cinfo, &outJpeg, &outSize);
	cinfo.image_width = width;//设置输入图片宽度
	cinfo.image_height = height;//设置图片高度
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_YCbCr;//设置输入图片的格式，支持RGB/YUV/YCC等等
	cinfo.dct_method = JDCT_FLOAT;
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
static void v4l2_read(void)
{
 char filename[12];
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
 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 buf.memory = V4L2_MEMORY_MMAP;
 for(;;)
 {
    for(buf.index=0;buf.index<frame_buffer_count;buf.index++)
    {
        base=screen_base;
        start=buf_infos[buf.index].start;
        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf); //出队
        //printf("%dsbdad\n",save_flag);
        if(save_flag)//(int width, int height, unsigned char *inputYuv,unsigned char *outJpeg)
        {
        unsigned long estimatedSize = width * height * 2+100;
     unsigned char *outJpeg = (unsigned char *)malloc(estimatedSize);
        if (outJpeg == NULL) {
        perror("Failed to allocate memory for JPEG data");
        return ;
        }
        out_size= yuv420sp_to_jpg(frm_width,frm_height,(unsigned char *) start,outJpeg);
         snprintf(filename, sizeof(filename), "./%d%s", count_file, JPEG_EXT);

        // 使用 open 创建文件，O_CREAT | O_WRONLY | O_TRUNC 表示创建文件，若文件存在则覆盖
        int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, FILE_MODE);

        // 检查文件描述符是否有效
        if (fd < 0) {
            perror("Error creating file");
            return ;
        }
        printf("camera ok%d\n",count_file);
        write(fd,outJpeg,out_size);
        free(outJpeg);
        count_file++;
        close(fd);
        pthread_mutex_lock(&mutex); //互斥锁上锁
        save_flag=0;
        pthread_mutex_unlock(&mutex); //互斥锁上锁
        }
        else
        for(int i=0;i<min_h;i++)
        {
        
                for(int l=0;l<min_w;l=l+2)
               yuv_rgb(start+l,base+l);
      
    
            base+=width;
            start+=frm_width;
        }
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
    }
 }
}
static void * save_picture(void*arg )
{
    char a;
    while(1){
    scanf("%c",&a);
    pthread_mutex_lock(&mutex); //互斥锁上锁
     printf("%c",a);
    if(a=='s')
    save_flag=1;

    a='l';
    printf("%c%d\n",a,save_flag);
    pthread_mutex_unlock(&mutex);//互斥锁解锁
    }
}
/********************************************/
int main(int argc,char **argv)
{
    pthread_attr_t attr;
     pthread_t tid;
     int  ret;
    if (2 != argc) {
        fprintf(stderr, "Usage: %s <video_dev>\n", argv[0]);
        exit(EXIT_FAILURE);
     }
    if (fb_dev_init())
        exit(EXIT_FAILURE);
 /* 初始化摄像头 */
 if (camera_init(argv[1]))
 exit(EXIT_FAILURE);
 /* 枚举所有格式并打印摄像头支持的分辨率及帧率 */

 v4l2_print_formats();
 /* 设置格式 */

 if (v4l2_set_format())
 exit(EXIT_FAILURE);

 if (v4l2_init_buffer())
 exit(EXIT_FAILURE);

 /* 开启视频采集 */
 if (v4l2_stream_on())
 exit(EXIT_FAILURE);
 /* 读取数据：出队 */
 pthread_mutex_init(&mutex, NULL);
 pthread_attr_init(&attr);
 /* 设置以分离状态启动线程 */
 pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
ret=pthread_create(&tid,&attr,save_picture,NULL);

 v4l2_read(); //在函数内循环采集数据、将其显示到 LCD 屏
 pthread_attr_destroy(&attr);
 exit(EXIT_SUCCESS);
}

