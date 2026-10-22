#include "shared.h"

typedef struct {
  uint8 tr_level[2];  /* TR pin output level */
  uint8 th_level[2];  /* TH pin output level */
  uint8 tr_dir[2];    /* TR pin direction */
  uint8 th_dir[2];    /* TH pin direction */
} io_state;

static io_state io_lut[2][256];
static io_state *io_current;

void pio_init(void)
{
  int i, j;

  for(j = 0; j < 2; j++)
  {
    for(i = 0; i < 0x100; i++)
    {
      io_lut[j][i].tr_dir[0]   = (i & 0x01) ? PIN_DIR_IN : PIN_DIR_OUT;
      io_lut[j][i].th_dir[0]   = (i & 0x02) ? PIN_DIR_IN : PIN_DIR_OUT;
      io_lut[j][i].tr_dir[1]   = (i & 0x04) ? PIN_DIR_IN : PIN_DIR_OUT;
      io_lut[j][i].th_dir[1]   = (i & 0x08) ? PIN_DIR_IN : PIN_DIR_OUT;

      if(j == 1)
      {
        io_lut[j][i].tr_level[0] = (i & 0x01) ? PIN_LVL_HI : (i & 0x10) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].th_level[0] = (i & 0x02) ? PIN_LVL_HI : (i & 0x20) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].tr_level[1] = (i & 0x04) ? PIN_LVL_HI : (i & 0x40) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].th_level[1] = (i & 0x08) ? PIN_LVL_HI : (i & 0x80) ? PIN_LVL_HI : PIN_LVL_LO;
      }
      else
      {
        io_lut[j][i].tr_level[0] = (i & 0x01) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].th_level[0] = (i & 0x02) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].tr_level[1] = (i & 0x04) ? PIN_LVL_HI : PIN_LVL_LO;
        io_lut[j][i].th_level[1] = (i & 0x08) ? PIN_LVL_HI : PIN_LVL_LO;
      }
    }
  }
}

void pio_reset(void)
{
  /* GG SIO power-on defaults */
  sms.sio.pdr     = 0x7F;
  sms.sio.ddr     = 0xFF;
  sms.sio.txdata  = 0x00;
  sms.sio.rxdata  = 0xFF;
  sms.sio.sctrl   = 0x00;

  io_current = &io_lut[sms.territory][0xFF];
  pio_ctrl_w(0xFF);
}

void pio_shutdown(void)
{
  /* Nothing to do */
}

void pio_ctrl_w(uint8 data)
{
  uint8 th_level[2];

  th_level[0] = io_current->th_level[0];
  th_level[1] = io_current->th_level[1];

  io_current = &io_lut[sms.territory][data];
  if ((io_current->th_dir[0]   == PIN_DIR_IN) &&
       (io_current->th_level[0] == PIN_LVL_HI) &&
       (th_level[0] == PIN_LVL_LO)
      )
  {
    sms.hlatch = hc_256[z80_get_elapsed_cycles() % CYCLES_PER_LINE];
  }

  sms.ioctrl = data;
}

static uint8 device_r(int port)
{
  uint8 temp = 0x7F;

  if (sms.device[port] == DEVICE_PAD2B)
  {
    if(input.pad[port] & INPUT_UP)      temp &= ~0x01;
    if(input.pad[port] & INPUT_DOWN)    temp &= ~0x02;
    if(input.pad[port] & INPUT_LEFT)    temp &= ~0x04;
    if(input.pad[port] & INPUT_RIGHT)   temp &= ~0x08;
    if(input.pad[port] & INPUT_BUTTON1) temp &= ~0x10;  /* TL */
    if(input.pad[port] & INPUT_BUTTON2) temp &= ~0x20;  /* TR */
  }

  return temp;
}

uint8 pio_port_r(int offset)
{
  uint8 temp = 0xFF;

  if(sms.memctrl & 0x04) return z80_read_unmapped();

  switch (offset & 1)
  {
    case 0:
      temp = device_r(0) & 0x3f;
      /* Read I/O port B low pins (Game Gear is special case) */
      if (IS_GG) temp |= ((sio_r(1) & 0x03) << 6);
      else temp |= ((device_r(1) & 0x03) << 6);

      if(io_current->tr_dir[0] == PIN_DIR_OUT)
      {
        temp &= ~0x20;
        temp |= (io_current->tr_level[0] == PIN_LVL_HI) ? 0x20 : 0x00;
      }
      break;

    case 1:
      if (IS_GG)
      {
        uint8 state = sio_r(0x01);
        temp = (state & 0x3C) >> 2;
        temp |= ((state & 0x40) << 1);
        temp |= 0x40;
      }
      else
      {
        uint8 state = device_r(1);
        temp = (state & 0x3C) >> 2;
        temp |= ((state & 0x40) << 1);
        temp |= (device_r(0) & 0x40);
      }

      if(io_current->tr_dir[1] == PIN_DIR_OUT)
      {
        temp &= ~0x08;
        temp |= (io_current->tr_level[1] == PIN_LVL_HI) ? 0x08 : 0x00;
      }

      if(io_current->th_dir[0] == PIN_DIR_OUT)
      {
        temp &= ~0x40;
        temp |= (io_current->th_level[0] == PIN_LVL_HI) ? 0x40 : 0x00;
      }

      if(io_current->th_dir[1] == PIN_DIR_OUT)
      {
        temp &= ~0x80;
        temp |= (io_current->th_level[1] == PIN_LVL_HI) ? 0x80 : 0x00;
      }

      temp |= 0x30;
      if (input.system & INPUT_RESET) temp &= ~0x10;
      break;
  }
  return temp;
}

uint8 sio_r(int offset)
{
  uint8 temp;

  switch(offset & 0xFF)
  {
    case 0:
      temp = 0xE0;
      if(input.system & INPUT_START)          temp &= ~0x80;
      if(sms.territory == TERRITORY_DOMESTIC) temp &= ~0x40;
      if(sms.display == DISPLAY_NTSC)         temp &= ~0x20;
      return temp;

    case 1:
      temp = 0x00;
      temp |= (sms.sio.ddr & 0x01) ? 0x01 : (sms.sio.pdr & 0x01);
      temp |= (sms.sio.ddr & 0x02) ? 0x02 : (sms.sio.pdr & 0x02);
      temp |= (sms.sio.ddr & 0x04) ? 0x04 : (sms.sio.pdr & 0x04);
      temp |= (sms.sio.ddr & 0x08) ? 0x08 : (sms.sio.pdr & 0x08);
      temp |= (sms.sio.ddr & 0x10) ? 0x10 : (sms.sio.pdr & 0x10);
      temp |= (sms.sio.ddr & 0x20) ? 0x20 : (sms.sio.pdr & 0x20);
      temp |= (sms.sio.ddr & 0x40) ? 0x40 : (sms.sio.pdr & 0x40);
      temp |= (sms.sio.pdr & 0x80);
      return temp;

    case 2:
      return sms.sio.ddr;

    case 3:
      return sms.sio.txdata;

    case 4:
      return sms.sio.rxdata;

    case 5:
      return sms.sio.sctrl;

    case 6:
      return 0xFF;
  }

  return 0xFF;
}

void sio_w(int offset, int data)
{
  switch(offset & 0xFF)
  {
    case 0:
      return;
    case 1:
      sms.sio.pdr = data;
      return;
    case 2:
      sms.sio.ddr = data;
      return;
    case 3:
      sms.sio.txdata = data;
      return;
    case 4:
      return;
    case 5:
      sms.sio.sctrl = data & 0xF8;
      return;
    case 6:
      psg_stereo_w(data);
      return;
  }
}