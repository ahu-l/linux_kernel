#ifndef _LCD_LZY_H
#define _LCD_LZY_H

#include <stdint.h> 

extern unsigned long screen_size;


int fb_dev_init(void);
unsigned int yuv420sp_to_jpg(int width, int height, unsigned char *inputYuv,FILE *fd);



#endif 


