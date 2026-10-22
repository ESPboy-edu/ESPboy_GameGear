#include "shared.h"

sms_t sms;
slot_t slot;

uint8 dummy_memory[0x400];

// Обработчики записи в память (RAM)
static void writemem_mapper_sega(int offset, int data)
{
  if(offset >= 0xFFFC)
  {
    mapper_16k_w(offset & 3, data);
  }
  cpu_writemap[offset >> 10][offset & 0x03FF] = data;
}

void mapper_reset(void)
{
  cpu_writemem16 = writemem_mapper_sega;
}

void sms_init(void)
{
  z80_init(0, 0, 0, sms_irq_callback);

  /* Для Game Gear используем свои порты ввода-вывода */
  cpu_writeport16 = gg_port_w;
  cpu_readport16 = gg_port_r;
  data_bus_pullup = 0xFF; // Подтяжка шины для GG
}

void sms_shutdown(void)
{
  /* Nothing to do */
}

void sms_reset(void)
{
  int i;

  z80_reset();
  z80_reset_cycle_count();
  z80_set_irq_line(0, CLEAR_LINE);

  /* Очищаем контекст SMS */
  memset(dummy_memory, data_bus_pullup, sizeof(dummy_memory));
  memset(sms.wram, 0, sizeof(sms.wram));
  sms.paused    = 0x00;
  sms.ioctrl    = 0xFF;
  sms.hlatch    = 0x00;
  sms.memctrl   = 0xAB;

  /* Подключаем картридж (указывает на PROGMEM) */
  slot.rom      = (uint8*)cart.rom;
  slot.pages    = cart.pages;
  slot.mapper   = cart.mapper;
  slot.fcr      = &cart.fcr[0];

  sms.wram[0] = sms.memctrl;

  /* Дефолтный маппинг ROM $0000-$BFFF */
  for(i = 0x00; i <= 0x2F; i++)
  {
    cpu_readmap[i]  = &slot.rom[(i & 0x1F) << 10];
    cpu_writemap[i] = dummy_memory; // В ROM писать нельзя
  }

  /* Включаем внутреннюю RAM $C000-$FFFF (8k, зеркалируется) */
  for(i = 0x30; i <= 0x3F; i++)
  {
      cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
      cpu_writemap[i] = &sms.wram[(i & 0x07) << 10];
  }

  /* Сброс регистров страниц памяти (SEGA Mapper) */
  cart.fcr[0] = 0;
  cart.fcr[1] = 0;
  cart.fcr[2] = 1;
  cart.fcr[3] = 2;

  mapper_reset();
  mapper_16k_w(0, slot.fcr[0]);
  mapper_16k_w(1, slot.fcr[1]);
  mapper_16k_w(2, slot.fcr[2]);
  mapper_16k_w(3, slot.fcr[3]);
}

void mapper_16k_w(int address, int data)
{
  int i;
  uint8 page = data % slot.pages;

  /* Сдвиг страниц для SEGA маппера */
  if (slot.fcr[0] & 0x03)
  {
    page = (page + ((4 - (slot.fcr[0] & 0x03)) << 3)) % slot.pages;
  }

  slot.fcr[address] = data;

  switch (address)
  {
    case 0:
    {
      if(data & 0x08)
      {
        /* SRAM (Saves) */
        int offset = (data & 0x04) ? 0x4000 : 0x0000;
        for(i = 0x20; i <= 0x2F; i++) {
          if (cart.sram) {
            cpu_readmap[i] = cpu_writemap[i] = &cart.sram[offset + ((i & 0x0F) << 10)];
          } else {
            cpu_readmap[i] = cpu_writemap[i] = dummy_memory;
          }
        }
      }
      else
      {
        page = slot.fcr[3] % slot.pages;
        if (slot.fcr[0] & 0x03) page = (page + ((4 - (slot.fcr[0] & 0x03)) << 3)) % slot.pages;

        /* ROM на $8000-$BFFF */
        for(i = 0x20; i <= 0x2F; i++) {
          cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
          cpu_writemap[i] = dummy_memory;
        }
      }

      if(data & 0x10)
      {
        for(i = 0x30; i <= 0x3F; i++) {
          if (cart.sram) {
            cpu_writemap[i] = cpu_readmap[i]  = &cart.sram[(i & 0x0F) << 10];
          } else {
            cpu_writemap[i] = cpu_readmap[i] = dummy_memory;
          }
        }
      }
      else
      {
        /* RAM на $C000-$FFFF */
        for(i = 0x30; i <= 0x3F; i++) {
          cpu_writemap[i] = cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
        }
      }
      break;
    }

    case 1: /* ROM bank $0000-$3FFF */
      for(i = 0x01; i <= 0x0F; i++) cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
      break;

    case 2: /* ROM bank $4000-$7FFF */
      for(i = 0x10; i <= 0x1F; i++) cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
      break;

    case 3: /* ROM bank $8000-$BFFF */
      if ((slot.fcr[0] & 0x08)) break; // SRAM active
      for(i = 0x20; i <= 0x2F; i++) cpu_readmap[i] = &slot.rom[(page << 14) | ((i & 0x0F) << 10)];
      break;
  }
}

int sms_irq_callback(int param)
{
  return 0xFF;
}