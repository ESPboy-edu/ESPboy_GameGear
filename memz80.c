#include "shared.h"

uint8 data_bus_pullup = 0xFF;

/* Чтение неразмеченной памяти (возвращаем мусор или pull-up) */
uint8 z80_read_unmapped(void)
{
  uint16 pc = (Z80.pc.w.l - 1) & 0xFFFF;
  // Используем pgm_read_byte для совместимости с PROGMEM-указателями
  uint8 data = pgm_read_byte(cpu_readmap[pc >> 10] + (pc & 0x03FF));
  return (data | data_bus_pullup);
}

/* Порт $3E (Memory Control Port) */
void memctrl_w(uint8 data)
{
  slot.rom = (uint8*)cart.rom;
  slot.mapper = cart.mapper;
  slot.pages = cart.pages;
  slot.fcr = &cart.fcr[0];

  mapper_reset();

  /* Сброс маппинга */
  if (slot.rom)
  {
    cpu_readmap[0] = &slot.rom[0];
    mapper_16k_w(0, slot.fcr[0]);
    mapper_16k_w(1, slot.fcr[1]);
    mapper_16k_w(2, slot.fcr[2]);
    mapper_16k_w(3, slot.fcr[3]);
  }

  sms.memctrl = data;
}

/*--------------------------------------------------------------------------*/
/* Game Gear port handlers                                                  */
/*--------------------------------------------------------------------------*/
void gg_port_w(uint16 port, uint8 data)
{
  port &= 0xFF;

  if(port <= 0x20) {
    sio_w(port, data);
    return;
  }

  switch(port & 0xC1)
  {
    case 0x00:
      memctrl_w(data);
      return;

    case 0x01:
      pio_ctrl_w(data);
      return;

    case 0x40:
    case 0x41:
      psg_write(data);
      return;

    case 0x80:
    case 0x81:
      gg_vdp_write(port, data);
      return;
  }
}

uint8 gg_port_r(uint16 port)
{
  port &= 0xFF;

  if(port <= 0x20)
    return sio_r(port);

  switch(port & 0xC0)
  {
    case 0x00:
      return z80_read_unmapped();

    case 0x40:
      return vdp_counter_read(port);

    case 0x80:
      return vdp_read(port);

    case 0xC0:
      switch(port)
      {
        case 0xC0:
        case 0xC1:
        case 0xDC:
        case 0xDD:
          return pio_port_r(port);
      }
      return z80_read_unmapped();
  }

  return 0xFF;
}