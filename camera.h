#ifndef _CAMERA_H
#define _CAMERA_H
#include <stdio.h>
#include <stdlib.h>

#define frame_buffer_count 3


typedef struct camera_format
{
    unsigned char description[32];
    unsigned int pixelformat;
} cam_fmt;
typedef struct cam_buf_info{
unsigned short * start;
unsigned long length;
}cam_buf_info;
void v4l2_read(void);

void  camera_prepare_init(char ** argv);



#endif

