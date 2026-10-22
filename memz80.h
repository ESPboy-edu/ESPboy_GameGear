#ifndef _MEMZ80_H_
#define _MEMZ80_H_

/* Global data */
extern uint8 data_bus_pullup;

/* Function prototypes */
extern uint8 z80_read_unmapped(void);

/* Оставлены только порты Game Gear */
extern void gg_port_w(uint16 port, uint8 data);
extern uint8 gg_port_r(uint16 port);

#endif /* _MEMZ80_H_ */