#ifndef _VDP_H_
#define _VDP_H_

/* VDP context */
typedef struct
{
    uint8 vram[0x4000];
    uint8 cram[0x40];
    uint8 reg[0x10];

    uint8 vscroll;
    uint8 status;
    uint8 latch;
    uint8 pending;

    uint16 addr;
    uint8 code;
    uint8 buffer;

    int pn;
    int ct;
    int pg;
    int sa;
    int sg;
    int ntab;
    int satb;
    int line;
    int left;

    uint16 lpf;
    uint8 height;
    uint8 extended;

    uint8 mode;
    uint8 irq;
    uint8 vint_pending;
    uint8 hint_pending;

    uint16 cram_latch;
    uint16 spr_col;

    uint8 spr_ovr;
    uint8 bd;
    uint8 _pad00;
    uint8 _pad01;
} __attribute__((aligned(4))) vdp_t;

/* Global data */
extern vdp_t vdp;
extern const uint8 hc_256[228] __attribute__((aligned(4)));
extern const uint8 *vc_table[3] __attribute__((aligned(4)));

/* Function prototypes */
extern void vdp_init(void);
extern void vdp_shutdown(void);
extern void vdp_reset(void);
extern uint8 vdp_read(int offset);
extern void vdp_write(int offset, uint8 data);
extern void gg_vdp_write(int offset, uint8 data);

static inline uint8 vdp_counter_read(int offset)
{
    if (offset & 1)  /* V Counter */
      return sms.hlatch;
    else /* H Counter -- прямое чтение из RAM */
      return vc_table[vdp.extended][z80_get_elapsed_cycles() / CYCLES_PER_LINE];
}

#endif /* _VDP_H_ */