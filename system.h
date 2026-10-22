#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#define APP_NAME            "SMS Plus GG"
#define APP_VERSION         "1.3-ESPboy"

#define PALETTE_SIZE        0x20
#define PIXEL_MASK          0x1F

#define INPUT_UP            0x00000001
#define INPUT_DOWN          0x00000002
#define INPUT_LEFT          0x00000004
#define INPUT_RIGHT         0x00000008
#define INPUT_BUTTON1       0x00000010
#define INPUT_BUTTON2       0x00000020

#define INPUT_START         0x00000001
#define INPUT_PAUSE         0x00000002
#define INPUT_RESET         0x00000004

/* User input structure */
typedef struct {
  uint8 pad[2];
  int analog[2][2];
  uint8 system;
} input_t;

/* Game image structure */
typedef struct {
  const uint8 *rom; // Указатели (32 бит)
  uint8 *sram; 
  uint32 size;      // 32 бит
  uint32 crc;       // 32 бит
  uint8 loaded;     // 8 бит
  uint8 pages;      // 8 бит
  int8 mapper;      // 8 бит
  uint8 fcr[4];     // Массив
  uint8 _pad;       // Ручное выравнивание до кратного 4
} cart_t;

/* Bitmap structure (без буфера data) */
typedef struct {
  int width;
  int height;
  int pitch;
  struct {
    int x, y, w, h;
    int ox, oy, ow, oh;
    int changed;
  } viewport;
} bitmap_t;

/* Урезанная структура опций */
typedef struct {
  int sndrate;
  int console;
  int display;
  uint8 overscan;
  uint8 spritelimit;
  uint8 extra_gg;
} option_t;

/* Global variables */
extern bitmap_t bitmap;
extern cart_t cart;
extern input_t input;
extern option_t option;

/* Function prototypes */
extern void system_frame(int skip);
extern void system_reset_config();
extern void system_init2(void);
extern void system_shutdown(void);
extern void system_reset(void);
extern void system_poweron(void);
extern void system_poweroff(void);

#endif /* _SYSTEM_H_ */