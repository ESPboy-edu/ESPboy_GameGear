#include "shared.h"

/* VDP context */
vdp_t vdp;

static void viewport_check(void)
{
  int i;
  int m1 = (vdp.reg[1] >> 4) & 1;
  int m3 = (vdp.reg[1] >> 3) & 1;
  int m2 = (vdp.reg[0] >> 1) & 1;
  int m4 = (vdp.reg[0] >> 2) & 1;

  vdp.mode = (m4 << 3 | m3 << 2 | m2 << 1 | m1 << 0);

  /* Всегда используем стандартную высоту 192 для GG/SMS (Mode 4) */
  vdp.height = 192;
  vdp.extended = 0;
  vdp.ntab = (vdp.reg[2] << 10) & 0x3800;

  if ((vdp.mode & 0x09) == 0x09) vdp.mode = 1;

  /* Видимая область для Game Gear всегда фиксирована (160x144) */
  bitmap.viewport.h = 144;
  bitmap.viewport.y = 0;
  bitmap.viewport.changed = 1;

  /* Восстанавливаем палитру */
  for(i = 0; i < PALETTE_SIZE; i++)
  {
    palette_sync(i);
  }

  vdp.pn = (vdp.reg[2] << 10) & 0x3C00;

  /* Всегда используем SMS/GG рендереры */
  render_bg  = render_bg_sms;
  render_obj = render_obj_sms;
}

void vdp_init(void)
{
  /* Устанавливаем область отрисовки Game Gear */
  bitmap.viewport.w = 160;
  bitmap.viewport.x = 48;
  vdp.lpf = sms.display ? 313 : 262;

  viewport_check();
}

void vdp_shutdown(void)
{
  // Пусто
}

void vdp_reset(void)
{
  memset(&vdp, 0, sizeof(vdp_t));

  vdp.lpf = sms.display ? 313 : 262;
  vdp.irq = INPUT_LINE_IRQ0;

  viewport_check();

  vdp.ct    = (vdp.reg[3] <<  6) & 0x3FC0;
  vdp.pg    = (vdp.reg[4] << 11) & 0x3800;
  vdp.satb  = (vdp.reg[5] << 7) & 0x3F00;
  vdp.sa    = (vdp.reg[5] <<  7) & 0x3F80;
  vdp.sg    = (vdp.reg[6] << 11) & 0x3800;
  vdp.bd    = (vdp.reg[7] & 0x0F);

  bitmap.viewport.changed = 1;
}

void vdp_reg_w(uint8 r, uint8 d)
{
  r &= 0xF;
  vdp.reg[r] = d;

  switch(r)
  {
    case 0x0: 
      if(vdp.hint_pending) {
        if(d & 0x10) z80_set_irq_line(0, ASSERT_LINE);
        else z80_set_irq_line(0, CLEAR_LINE);
      }
      viewport_check();
      break;

    case 0x1: 
      if(vdp.vint_pending) {
        if(d & 0x20) z80_set_irq_line(vdp.irq, ASSERT_LINE);
        else z80_set_irq_line(vdp.irq, CLEAR_LINE);
      }
      viewport_check();
      break;

    case 0x2: 
      viewport_check();
      break;

    case 0x3:
      vdp.ct = (d <<  6) & 0x3FC0;
      break;

    case 0x4:
      vdp.pg = (d << 11) & 0x3800;
      break;

    case 0x5: 
      vdp.satb = (d << 7) & 0x3F00;
      vdp.sa = (d <<  7) & 0x3F80;
      break;

    case 0x6:
      vdp.sg = (d << 11) & 0x3800;
      break;

    case 0x7:
      vdp.bd = (d & 0x0F);
      break;
  }
}

void vdp_write(int offset, uint8 data)
{
  int index;

  if (((z80_get_elapsed_cycles() + 1) / CYCLES_PER_LINE) > vdp.line)
  {
    render_line((vdp.line+1)%vdp.lpf);
  }

  if (offset & 1) /* Control port */
  {
    if(vdp.pending == 0)
    {
      vdp.addr = (vdp.addr & 0x3F00) | (data & 0xFF);
      vdp.latch = data;
      vdp.pending = 1;
    }
    else
    {
      vdp.pending = 0;
      vdp.code = (data >> 6) & 3;
      vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;

      if(vdp.code == 0)
      {
        vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
        vdp.addr = (vdp.addr + 1) & 0x3FFF;
      }
      else if(vdp.code == 2)
      {
        vdp_reg_w(data & 0xF, vdp.latch);
      }
    }
  }
  else /* Data port */
  {
    vdp.pending = 0;

    switch(vdp.code)
    {
      case 0: case 1: case 2: 
        vdp.vram[(vdp.addr & 0x3FFF)] = data;
        vdp.buffer = data;
        break;

      case 3: /* CRAM write */
        index = (vdp.addr & 0x1F);
        if(data != vdp.cram[index])
        {
          vdp.cram[index] = data;
          palette_sync(index);
        }
        vdp.buffer = data;
        break;
    }
    vdp.addr = (vdp.addr + 1) & 0x3FFF;
  }
}

uint8 vdp_read(int offset)
{
  uint8 temp;

  if (offset & 1) /* Status flags */
  {
    int cyc   = z80_get_elapsed_cycles();
    int line  = vdp.line;
    if ((cyc / CYCLES_PER_LINE) > line)
    {
      if (line == vdp.height) vdp.status |= 0x80;
      line = (line + 1)%vdp.lpf;
      render_line(line);
    }

    temp = vdp.status | 0x1f;

    vdp.status = 0;
    vdp.pending = 0;
    vdp.vint_pending = 0;
    vdp.hint_pending = 0;
    z80_set_irq_line(vdp.irq, CLEAR_LINE);

    if (temp & 0x20)
    {
      // Прямое чтение hc_256 из RAM
      uint8 hc = hc_256[(cyc + 1) % CYCLES_PER_LINE];
      if ((line == (vdp.spr_col >> 8)) && ((hc < (vdp.spr_col & 0xff)) || (hc > 0xf3)))
      {
        vdp.status |= 0x20;
        temp &= ~0x20;
      }
    }
  }
  else /* Data buffer */
  {
    vdp.pending = 0;
    temp = vdp.buffer;
    vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
    vdp.addr = (vdp.addr + 1) & 0x3FFF;
  }

  return temp;
}

void gg_vdp_write(int offset, uint8 data)
{
  if (((z80_get_elapsed_cycles() + 1) / CYCLES_PER_LINE) > vdp.line)
  {
    render_line((vdp.line+1)%vdp.lpf);
  }

  if (offset & 1) /* Control port */
  {
    if(vdp.pending == 0)
    {
      vdp.addr = (vdp.addr & 0x3F00) | (data & 0xFF);
      vdp.latch = data;
      vdp.pending = 1;
    }
    else
    {
      vdp.pending = 0;
      vdp.code = (data >> 6) & 3;
      vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;

      if(vdp.code == 0)
      {
        vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
        vdp.addr = (vdp.addr + 1) & 0x3FFF;
      }
      else if(vdp.code == 2)
      {
        vdp_reg_w(data & 0xF, vdp.latch);
      }
    }
  }
  else /* Data port */
  {
    vdp.pending = 0;
    switch(vdp.code)
    {
      case 0: case 1: case 2: 
        vdp.vram[(vdp.addr & 0x3FFF)] = data;
        vdp.buffer = data;
        break;

      case 3: /* CRAM write */
        if(vdp.addr & 1)
        {
          vdp.cram_latch = (vdp.cram_latch & 0x00FF) | ((data & 0xFF) << 8);
          vdp.cram[(vdp.addr & 0x3E) | (0)] = (vdp.cram_latch >> 0) & 0xFF;
          vdp.cram[(vdp.addr & 0x3E) | (1)] = (vdp.cram_latch >> 8) & 0xFF;
          palette_sync((vdp.addr >> 1) & 0x1F);
        }
        else
        {
          vdp.cram_latch = (vdp.cram_latch & 0xFF00) | ((data & 0xFF) << 0);
        }
        vdp.buffer = data;
        break;
    }
    vdp.addr = (vdp.addr + 1) & 0x3FFF;
  }
}