#include "shared.h"
#include "hvc.h"

#pragma GCC optimize ("O3")

// Супербыстрое смешивание двух RGB565 цветов 50/50 за 2 такта
#define BLEND_50(c1, c2) (((c1 & c2) + (((c1 ^ c2) & 0xDEF6) >> 1)))


// Флаг из .ino файла
extern uint8_t scale_fullscreen;

// LUT-таблицы для масштабирования (160x144 -> 128x128)
static uint8_t scale_x_lut[128] __attribute__((aligned(4)));
static uint8_t scale_y_lut[192] __attribute__((aligned(4)));
static uint8_t render_this_gg_line[192] __attribute__((aligned(4)));


// === НАСТРОЙКИ ЭКРАНА И ЧАНКОВ ===
// Смещение экрана по вертикали (допустимы значения от 0 до 16):
//  0  = сдвинуть картинку вниз (виден самый верх игры, обрезано снизу)
//  8  = отцентрировать (обрезано по 8 пикселей сверху и снизу)
//  16 = сдвинуть картинку вверх (виден самый низ игры, обрезано сверху)
#define ESPBOY_Y_OFFSET 0 

// Магическое число 24 - это первая видимая строка на реальном экране Game Gear
#define VDP_START_LINE (24 + ESPBOY_Y_OFFSET)
#define VDP_END_LINE   (VDP_START_LINE + 128)

#define CHUNK_LINES 16
static uint16_t spi_chunk_buffer[2][CHUNK_LINES * 128] __attribute__((aligned(4)));
static uint8_t pingpong = 0;
static uint8_t chunk_line_count = 0;
static int chunk_y_start = 0;

struct
{
  uint16_t yrange;
  uint16_t xpos;
  uint16_t attr;
  uint16_t _pad0;
} object_info[64];

/* Background drawing function */
void (*render_bg)(int line) = NULL;
void (*render_obj)(int line) = NULL;

/* Pointer to output buffer */
uint8_t *linebuf;

/* Internal buffer for drawing 8-bit displays */
static uint8_t internal_buffer[512] __attribute__((aligned(4))); // Добавлено выравнивание

/* Precalculated pixel table */
static uint16_t pixel[PALETTE_SIZE] __attribute__((aligned(4)));

/* Region-specific drawing area */
static int active_scanlines;
static int active_border[3];
const uint8_t *vc_table[3];

static uint8_t object_index_count;
static int prev_line = -1;
static int skip_render = 0;

// === НАСТРОЙКИ ЧАНКОВ И LUT ===
// (Удален дублирующийся CHUNK_LINES, он уже есть вверху файла)

// Таблица предрасчета прозрачности и приоритетов спрайтов
static uint8_t merge_lut[256][16] __attribute__((aligned(4)));

/* ========================================================================= */
/* Распаковка битовых плоскостей на лету                                     */
/* ========================================================================= */
static uint8_t tile_data_cache[8] __attribute__((aligned(4)));

// ИСПОЛЬЗУЕМ int вместо short для 32-битного процессора
static inline uint8_t* tile_get(int attr, int line) 
{
    const uint16_t name = attr & 0x1ff;
    const uint16_t y = (attr & 0x400) ? (line ^ 7) : line;
    uint16_t addr = (name << 5) | (y << 2);
    
    uint32_t *vram32 = (uint32_t *)&vdp.vram[addr];
    uint32_t data = *vram32; // Читаем сразу 4 байта (d0, d1, d2, d3) за 1 такт!
    
    uint8_t d0 = data & 0xFF;
    uint8_t d1 = (data >> 8) & 0xFF;
    uint8_t d2 = (data >> 16) & 0xFF;
    uint8_t d3 = (data >> 24) & 0xFF;

    if (attr & 0x200) { // flip_x
        tile_data_cache[7] = ((d0 >> 7) & 1) | (((d1 >> 7) & 1) << 1) | (((d2 >> 7) & 1) << 2) | (((d3 >> 7) & 1) << 3);
        tile_data_cache[6] = ((d0 >> 6) & 1) | (((d1 >> 6) & 1) << 1) | (((d2 >> 6) & 1) << 2) | (((d3 >> 6) & 1) << 3);
        tile_data_cache[5] = ((d0 >> 5) & 1) | (((d1 >> 5) & 1) << 1) | (((d2 >> 5) & 1) << 2) | (((d3 >> 5) & 1) << 3);
        tile_data_cache[4] = ((d0 >> 4) & 1) | (((d1 >> 4) & 1) << 1) | (((d2 >> 4) & 1) << 2) | (((d3 >> 4) & 1) << 3);
        tile_data_cache[3] = ((d0 >> 3) & 1) | (((d1 >> 3) & 1) << 1) | (((d2 >> 3) & 1) << 2) | (((d3 >> 3) & 1) << 3);
        tile_data_cache[2] = ((d0 >> 2) & 1) | (((d1 >> 2) & 1) << 1) | (((d2 >> 2) & 1) << 2) | (((d3 >> 2) & 1) << 3);
        tile_data_cache[1] = ((d0 >> 1) & 1) | (((d1 >> 1) & 1) << 1) | (((d2 >> 1) & 1) << 2) | (((d3 >> 1) & 1) << 3);
        tile_data_cache[0] = (d0 & 1) | ((d1 & 1) << 1) | ((d2 & 1) << 2) | ((d3 & 1) << 3);
    } else {
        tile_data_cache[0] = ((d0 >> 7) & 1) | (((d1 >> 7) & 1) << 1) | (((d2 >> 7) & 1) << 2) | (((d3 >> 7) & 1) << 3);
        tile_data_cache[1] = ((d0 >> 6) & 1) | (((d1 >> 6) & 1) << 1) | (((d2 >> 6) & 1) << 2) | (((d3 >> 6) & 1) << 3);
        tile_data_cache[2] = ((d0 >> 5) & 1) | (((d1 >> 5) & 1) << 1) | (((d2 >> 5) & 1) << 2) | (((d3 >> 5) & 1) << 3);
        tile_data_cache[3] = ((d0 >> 4) & 1) | (((d1 >> 4) & 1) << 1) | (((d2 >> 4) & 1) << 2) | (((d3 >> 4) & 1) << 3);
        tile_data_cache[4] = ((d0 >> 3) & 1) | (((d1 >> 3) & 1) << 1) | (((d2 >> 3) & 1) << 2) | (((d3 >> 3) & 1) << 3);
        tile_data_cache[5] = ((d0 >> 2) & 1) | (((d1 >> 2) & 1) << 1) | (((d2 >> 2) & 1) << 2) | (((d3 >> 2) & 1) << 3);
        tile_data_cache[6] = ((d0 >> 1) & 1) | (((d1 >> 1) & 1) << 1) | (((d2 >> 1) & 1) << 2) | (((d3 >> 1) & 1) << 3);
        tile_data_cache[7] = (d0 & 1) | ((d1 & 1) << 1) | ((d2 & 1) << 2) | ((d3 & 1) << 3);
    }
    return tile_data_cache;
}
/* ========================================================================= */

void render_shutdown(void) {}

void render_init(void) 
{
  // Генерируем LUT матрицу приоритетов (отрабатывает за 1 мс при старте игры)
  for (int bg = 0; bg < 256; bg++) {
    for (int sp = 0; sp < 16; sp++) {
      uint8_t b = bg & 0x0F;
      uint8_t bp = (bg & 0x20) ? 1 : 0;
      uint8_t bf = bg & 0x7F;
      uint8_t s = sp & 0x0F;
      uint8_t sf = s | 0x10 | 0x40; // Метка спрайта и смещение палитры

      if (bg & 0x40) {
        merge_lut[bg][sp] = bf; 
      } else if (bp) {
        if (b) merge_lut[bg][sp] = bf | 0x40; 
        else if (s) merge_lut[bg][sp] = sf;
        else merge_lut[bg][sp] = bf;
      } else {
        if (s) merge_lut[bg][sp] = sf;
        else merge_lut[bg][sp] = bf;
      }
    }
  }

  // --- Генерируем LUT для масштабирования ---
  // Расчет по X: выбираем 128 пикселей из 160 (пропуская каждый пятый)
  for (int i = 0; i < 128; i++) {
    scale_x_lut[i] = 48 + (i * 160) / 128; // 48 - это начало видимой зоны GG
  }
  
  // Расчет по Y: выбираем 128 строк из 144
  memset(render_this_gg_line, 0, sizeof(render_this_gg_line));
  for (int i = 0; i < 128; i++) {
    int gg_line = 24 + (i * 144) / 128; // 24 - верхняя граница экрана GG
    render_this_gg_line[gg_line] = 1;   // Отмечаем строку как нужную
    scale_y_lut[gg_line] = i;           // Запоминаем её номер на экране ESPboy
  }
}


void render_reset(void)
{
  int i;
  for(i = 0; i < PALETTE_SIZE; i++) palette_sync(i);

  render_bg = render_bg_sms;
  render_obj = render_obj_sms;

  if (sms.display == DISPLAY_NTSC) { // NTSC
    active_scanlines = 243;
    active_border[0] = 24;
    active_border[1] = 8;
    active_border[2] = 0;
    vc_table[0] = vc_ntsc_192;
    vc_table[1] = vc_ntsc_224;
    vc_table[2] = vc_ntsc_240;
  } else { // PAL
    active_scanlines = 293;
    active_border[0] = 48;
    active_border[1] = 32;
    active_border[2] = 24;
    vc_table[0] = vc_pal_192;
    vc_table[1] = vc_pal_224;
    vc_table[2] = vc_pal_240;
  }
}

void render_mode(int skip)
{
    skip_render = skip;
}

static inline void parse_satb(int line)
{
  uint8_t *st = (uint8_t *)&vdp.vram[vdp.satb];
  int i = 0;
  int vc = vc_table[vdp.extended][line];
  int yp;
  int height = (vdp.reg[1] & 0x02) ? 16 : 8;

  if(vdp.reg[1] & 0x01) height <<= 1;

  object_index_count = 0;

  for(i = 0; i < 64; i++)
  {
    yp = st[i];
    if(vdp.extended == 0 && yp == 208) return;
    if(yp > 240) yp -= 256;
    
    yp = vc - yp;

    if((yp >= 0) && (yp < height))
    {
      if (object_index_count == 8) {
        if (line < vdp.height) vdp.spr_ovr = 1;
        if (option.spritelimit) return;
      }

      object_info[object_index_count].yrange = yp;
      object_info[object_index_count].xpos = st[0x80 + (i << 1)];
      object_info[object_index_count].attr = st[0x81 + (i << 1)];
      ++object_index_count;
    }
  }
}


void render_line(int line)
{
  if (prev_line == line) return;
  prev_line = line;

  if (line >= 192) return; // VDP рендерит максимум 192 строки

  linebuf = &internal_buffer[0];

  if (vdp.spr_ovr) {
    vdp.spr_ovr = 0;
    vdp.status |= 0x40;
  }

  // Отрисовка строки 
  if (vdp.reg[1] & 0x40) {
    render_bg_sms(line);
    render_obj_sms(line);
  } else {
    memset(linebuf, BACKDROP_COLOR, 256);
  }

  if (vdp.mode > 7) parse_satb(line);

  /* Отправка на экран ESPboy (128x128) */
  if (!skip_render) {
    if (scale_fullscreen) {
        // --- РЕЖИМ FULLSCREEN (160x144 -> 128x128 с псевдо-AA) ---
        if (line >= 24 && line < 168 && render_this_gg_line[line]) {
            int y = scale_y_lut[line];

            if (chunk_line_count == 0) chunk_y_start = y;
            
            int offset = chunk_line_count * 128;
            uint16_t *dst16 = (uint16_t *)&spi_chunk_buffer[pingpong][offset];

            // Псевдо-сглаживание по горизонтали
            for (int x = 0; x < 128; x++) {
                int orig_x = scale_x_lut[x];
                
                // Берем целевой пиксель и его соседа справа
                uint8_t c1 = internal_buffer[orig_x] & PIXEL_MASK;
                uint8_t c2 = internal_buffer[orig_x + 1] & PIXEL_MASK;
                
                uint16_t p1 = pixel[c1];
                uint16_t p2 = pixel[c2];
                
                // Бранчлесс или быстрое ветвление: если цвета равны, пропускаем смешивание
                dst16[x] = (p1 == p2) ? p1 : BLEND_50(p1, p2);
            }

            chunk_line_count++;

            if (chunk_line_count >= CHUNK_LINES || y == 127) {
                draw_lcd_chunk(chunk_y_start, chunk_line_count, spi_chunk_buffer[pingpong]);
                pingpong ^= 1;
                chunk_line_count = 0;
            }
        }
    } else {
        // --- СТАНДАРТНЫЙ РЕЖИМ (Центрирование, 32-бит) ---
        if ((line >= VDP_START_LINE) && (line < VDP_END_LINE)) {
            int y = line - VDP_START_LINE;
            
            if (chunk_line_count == 0) chunk_y_start = y;
            
            int offset = chunk_line_count * 128;
            uint32_t *src32 = (uint32_t *)&internal_buffer[64];
            uint32_t *dst32 = (uint32_t *)&spi_chunk_buffer[pingpong][offset];

            for(int x = 0; x < 32; x++) {
                uint32_t quad_pixels = src32[x]; 
                
                uint8_t c0 = (quad_pixels & 0xFF) & PIXEL_MASK;
                uint8_t c1 = ((quad_pixels >> 8) & 0xFF) & PIXEL_MASK;
                uint8_t c2 = ((quad_pixels >> 16) & 0xFF) & PIXEL_MASK;
                uint8_t c3 = ((quad_pixels >> 24) & 0xFF) & PIXEL_MASK;

                dst32[x * 2]     = (pixel[c1] << 16) | pixel[c0];
                dst32[x * 2 + 1] = (pixel[c3] << 16) | pixel[c2];
            }
            
            chunk_line_count++;

            if (chunk_line_count >= CHUNK_LINES || y == 127) {
                draw_lcd_chunk(chunk_y_start, chunk_line_count, spi_chunk_buffer[pingpong]);
                pingpong ^= 1;
                chunk_line_count = 0;
            }
        }
    }
  }
}


void render_bg_sms(int line)
{
  int locked = 0;
  int yscroll_mask = (vdp.extended) ? 256 : 224;
  int v_line = (line + vdp.vscroll) % yscroll_mask;
  int v_row  = (v_line & 7) << 3;
  int hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10) && !IS_GG) ? 0 : (0x100 - vdp.reg[8]);
  int column = 0;
  uint16_t attr;
  uint16_t nt_addr = (vdp.ntab + ((v_line >> 3) << 6)) & 0xFFFF;
  uint16_t *nt = (uint16_t *)&vdp.vram[nt_addr];
  int nt_scroll = (hscroll >> 3);
  int shift = (hscroll & 7);
  uint8_t *cache_ptr;

  uint8_t *linebuf_ptr = &linebuf[0 - shift];

  if(shift) {
    for(int x = shift; x < 8; x++) linebuf_ptr[x] = 0;
    column++;
  }

  for(; column < 32; column++)
  {
    if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24)) {
      locked = 1;
      v_row = (line & 7) << 3;
      nt = (uint16_t *)&vdp.vram[nt_addr];
    }

    attr = nt[(column + nt_scroll) & 0x1F];

    uint8_t atex_mask = ((attr >> 11) & 3) << 4; 
    cache_ptr = tile_get(attr, v_row >> 3);

    // ВАЖНО: Убран ошибочный if (c). Фон всегда переписывает строку!
    for(int i = 0; i < 8; i++) {
        linebuf_ptr[(column << 3) + i] = cache_ptr[i] | atex_mask;
    }
  }

  if(shift) {
    attr = nt[(column + nt_scroll) & 0x1F];
    uint8_t atex_mask = ((attr >> 11) & 3) << 4;
    cache_ptr = tile_get(attr, v_row >> 3);

    uint8_t *p = &linebuf[(0 - shift) + (column << 3)];
    for(int x = 0; x < shift; x++) {
      p[x] = cache_ptr[x] | atex_mask;
    }
  }
}


void render_obj_sms(int line)
{
  int i, x, start, end, xp, yp, n;
  uint8_t sp, bg;
  uint8_t *linebuf_ptr;
  uint8_t *cache_ptr;

  int width = 8;
  if(vdp.reg[1] & 0x01) width *= 2;

  for(i = 0; i < object_index_count; i++)
  {
    start = 0;
    end = width;
    xp = object_info[i].xpos;
    yp = object_info[i].yrange;
    n = object_info[i].attr;

    if(vdp.reg[0] & 0x08) xp -= 8;
    if(vdp.reg[6] & 0x04) n |= 0x0100;
    if(vdp.reg[1] & 0x02) n &= 0x01FE;

    linebuf_ptr = &linebuf[xp];

    if(xp < 0) start = (0 - xp);
    if((xp + width) > 256) end = (256 - xp);

    if(vdp.reg[1] & 0x01) // Doubled size
    {
      cache_ptr = tile_get(n, yp >> 1);
      for(x = start; x < end; x+=2)
      {
        sp = cache_ptr[(x >> 1)];
        if(sp) // Быстрый выход, если пиксель прозрачный
        {
          bg = linebuf_ptr[x];
          // Branchless смешивание пикселей через RAM LUT (мгновенно!)
          linebuf_ptr[x] = linebuf_ptr[x+1] = merge_lut[bg][sp];

          if ((bg & 0x40) && !(vdp.status & 0x20)) {
            vdp.status |= 0x20;
            vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
          }
        }
      }
    }
    else // Normal size
    {
      cache_ptr = tile_get(n, yp);
      for(x = start; x < end; x++)
      {
        sp = cache_ptr[x];
        if(sp)
        {
          bg = linebuf_ptr[x];
          // Branchless смешивание пикселей через RAM LUT
          linebuf_ptr[x] = merge_lut[bg][sp];

          if ((bg & 0x40) && !(vdp.status & 0x20)) {
            vdp.status |= 0x20;
            vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
          }
        }
      }
    }
  }
}

void palette_sync(int index)
{
  int r, g, b;

  if (IS_GG) {
    r = (vdp.cram[(index << 1) | (0)] >> 0) & 0x0F;
    g = (vdp.cram[(index << 1) | (0)] >> 4) & 0x0F;
    b = (vdp.cram[(index << 1) | (1)] >> 0) & 0x0F;
    
    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;
  } else {
    r = (vdp.cram[index] >> 0) & 3;
    g = (vdp.cram[index] >> 2) & 3;
    b = (vdp.cram[index] >> 4) & 3;
    
    r = (r * 85); 
    g = (g * 85);
    b = (b * 85);
  }

  pixel[index] = MAKE_PIXEL(r, g, b);
}
