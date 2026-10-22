#ifndef _RENDER_H_
#define _RENDER_H_

/* Pre-swapped RGB 5:6:5 format for ESP8266 Little-Endian SPI */
#define MAKE_PIXEL(r,g,b) ( ((r & 0xF8) | ((g & 0xE0) >> 5)) | ((((g & 0x1C) << 3) | ((b & 0xF8) >> 3)) << 8) )

/* Used for blanking a line in whole or in part */
#define BACKDROP_COLOR      (0x10 | (vdp.reg[7] & 0x0F))

extern void (*render_bg)(int line);
extern void (*render_obj)(int line);
extern const uint8 *vc_table[3];
extern uint8 *linebuf;

extern void render_shutdown(void);
extern void render_init(void);
extern void render_reset(void);
extern void render_mode(int skip);
extern void render_line(int line);
extern void render_bg_sms(int line);
extern void render_obj_sms(int line);
extern void palette_sync(int index);

/* Внешняя функция, которая будет реализована в .ino файле для отправки SPI чанка */
extern void draw_lcd_chunk(int y, int lines, uint16_t *pixels);

#endif /* _RENDER_H_ */
