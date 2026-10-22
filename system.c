#include "shared.h"

bitmap_t bitmap;
cart_t cart;
input_t input;
option_t option;

#define SND_CHUNK_SIZE 512
int16_t snd_mono[SND_CHUNK_SIZE] __attribute__((aligned(4)));

extern void push_audio_sample(uint8_t sample); // Из ESPboy_GameGear.ino

/* Run the virtual console emulation for one frame */
void system_frame(int skip)
{
  int iline, line_z80 = 0;

  render_mode(skip);

  /* Debounce pause key */
  if(input.system & INPUT_PAUSE)
  {
    if(!sms.paused) {
      sms.paused = 1;
      z80_set_irq_line(INPUT_LINE_NMI, ASSERT_LINE);
      z80_set_irq_line(INPUT_LINE_NMI, CLEAR_LINE);
    }
  }
  else {
     sms.paused = 0;
  }

  vdp.vscroll = vdp.reg[9];
  vdp.left = vdp.reg[0x0A];
  vdp.spr_col = 0xff00;

  for(vdp.line = 0; vdp.line < vdp.lpf; vdp.line++)
  {
    iline = vdp.height;

    if(!skip) render_line(vdp.line);

    if(vdp.line <= iline)
    {
      if(--vdp.left < 0)
      {
        vdp.left = vdp.reg[0x0A];
        vdp.hint_pending = 1;
        if(vdp.reg[0x00] & 0x10)
        {
          if (!(z80_get_elapsed_cycles()%CYCLES_PER_LINE))
            z80_execute(1);
          z80_set_irq_line(0, ASSERT_LINE);
        }
      }
    }

    line_z80 += CYCLES_PER_LINE;
    z80_execute(line_z80 - z80_cycle_count);

    if(vdp.line == iline)
    {
      vdp.status |= 0x80;
      vdp.vint_pending = 1;
      if(vdp.reg[0x01] & 0x20) z80_set_irq_line(vdp.irq, ASSERT_LINE);
    }
  }

  z80_cycle_count -= line_z80;

  /* --- Стабильная генерация звука один раз за кадр --- */
  int samples_to_generate = option.sndrate / (sms.display == DISPLAY_NTSC ? 60 : 50);
  
  if (samples_to_generate > SND_CHUNK_SIZE) samples_to_generate = SND_CHUNK_SIZE;
  
  SN76489_Update(0, snd_mono, samples_to_generate);

  for (int i = 0; i < samples_to_generate; i++) {
    int16_t sample_8bit = (snd_mono[i] >> 6) + 128; 
    
    if (sample_8bit > 255) sample_8bit = 255;
    if (sample_8bit < 0) sample_8bit = 0;

    push_audio_sample((uint8_t)sample_8bit);
  }
}


void system_reset_config()
{
  option.sndrate      = 22000;         // По умолчанию 22 кГц
  option.console      = CONSOLE_GG;
  option.display      = DISPLAY_NTSC;
  option.overscan     = 0;
  option.spritelimit  = 1;
  option.extra_gg     = 0;
}

void system_init2(void)
{
  sms_init();
  pio_init();
  vdp_init();
  render_init();
  SN76489_Init(0, 3579545, option.sndrate); 
}

void system_shutdown(void)
{
  sms_shutdown();
  pio_shutdown();
  vdp_shutdown();
  render_shutdown();
  SN76489_Shutdown();
}

void system_reset(void)
{
  sms.display = option.display; // Синхронизируем NTSC/PAL
  sms.device[0] = DEVICE_PAD2B;
  sms.device[1] = DEVICE_PAD2B;
  sms_reset();
  pio_reset();
  vdp_reset();
  render_reset();
  SN76489_Reset(0);
}

void system_poweron(void)
{
  system_init2();
  system_reset();
}

void system_poweroff(void) {}
