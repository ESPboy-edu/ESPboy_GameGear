#ifndef _SMS_H_
#define _SMS_H_

#define CYCLES_PER_LINE 228

enum {
  SLOT_BIOS   = 0,
  SLOT_CARD   = 1,
  SLOT_CART   = 2,
  SLOT_EXP    = 3
};

enum {
  MAPPER_NONE       = 0,
  MAPPER_SEGA       = 1
};

enum {
  DISPLAY_NTSC    = 0,
  DISPLAY_PAL     = 1
};

enum {
  FPS_NTSC    = 60,
  FPS_PAL     = 50
};

enum {
  CLOCK_NTSC      = 3579545,
  CLOCK_PAL       = 3546895,
  CLOCK_NTSC_SMS1 = 3579527
};

enum {
  CONSOLE_SMS     = 0x20,
  CONSOLE_SMS2    = 0x21,
  CONSOLE_GG      = 0x40,
  CONSOLE_GGMS    = 0x41,
};

#define HWTYPE_SMS  CONSOLE_SMS
#define HWTYPE_GG   CONSOLE_GG

#define IS_SMS    (sms.console & HWTYPE_SMS)
#define IS_GG     (sms.console & HWTYPE_GG)

enum {
  TERRITORY_DOMESTIC  = 0,
  TERRITORY_EXPORT    = 1
};

/* SIO (Serial I/O) для Game Gear */
typedef struct {
  uint8 pdr;    /* Parallel data register */
  uint8 ddr;    /* Data direction register */
  uint8 txdata; /* Transmit data buffer */
  uint8 rxdata; /* Receive data buffer */

  uint8 sctrl;  /* Serial mode control and status */
  uint8 _pad00;
  uint8 _pad01;
  uint8 _pad02;
} __attribute__((aligned(4))) sio_t;

/* Главный контекст эмулятора */
typedef struct
{
  uint8 wram[0x2000];

  uint8 paused;
  uint8 save;
  uint8 territory;
  uint8 console;

  uint8 display;
  uint8 hlatch;

  uint8 memctrl;
  uint8 ioctrl;

  sio_t sio;

  uint8 device[2];
  uint8 gun_offset;
  uint8 _pad01;
}  __attribute__((aligned(4))) sms_t;

/* BIOS ROM (оставлен для совместимости с базовыми структурами) */
typedef struct
{
  uint8 *rom;
  uint8 enabled;
  uint8 pages;
  uint8 fcr[4];
}  __attribute__((aligned(4))) bios_t;

/* Данные маппера */
typedef struct
{
  uint8 *rom;
  uint8 pages;
  uint8 *fcr;
  uint8 mapper;
}  __attribute__((aligned(4))) slot_t;

/* Global data */
extern sms_t sms;
extern bios_t bios;
extern slot_t slot;
extern uint8 dummy_memory[0x400];

/* Function prototypes */
extern void sms_init(void);
extern void sms_reset(void);
extern void sms_shutdown(void);
extern void mapper_reset(void);
extern void mapper_8k_w(int address, int data);
extern void mapper_16k_w(int address, int data);
extern int sms_irq_callback(int param);

#endif /* _SMS_H_ */