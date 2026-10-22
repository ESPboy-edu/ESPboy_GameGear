#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"
#include "nbSPI.h"
#include <sigma_delta.h> 


// ==========================================
// ROM Данные
// ==========================================
//#include "games/rom1.h"    // Sonic the Hedgehog
//#include "games/rom2.h"  // Aladdin
//#include "games/rom3.h"  // Alien 3
//#include "games/rom4.h"  // Arena - Maze of Death
//#include "games/rom5.h"  // Ax Battler - A Legend of Golden Axe 
//#include "games/rom6.h"  // Battletoads
#include "games/rom7.h"  // Megaman 
//#include "games/rom8.h"  // Prince of Persia 
//#include "games/rom9.h"  // Ristar the Shooting Star 
//#include "games/rom10.h" // Sonic Drift 2 
//#include "games/rom11.h" // Sonic the Hedgehog - Triple Trouble



extern "C" {
  #include "shared.h"
}

ESPboyInit myESPboy;


// ==========================================
// Вывод графики (Чанки по SPI DMA/Interrupts)
// ==========================================
extern "C" void draw_lcd_chunk(int y, int lines, uint16_t *pixels) {
  while(nbSPI_isBusy()); 
  myESPboy.tft.setAddrWindow(0, y, 128, lines);
  nbSPI_writeBytes((uint8_t*)pixels, 128 * lines * 2);
}


// ==========================================
// Аудио Подсистема
// ==========================================
#define AUDIO_SAMPLE_RATE 11025 // Снижаем до 11 кГц. 
#define SOUNDPIN D3
#define AUDIO_BUFFER_SIZE 1024

volatile uint8_t audio_buffer[AUDIO_BUFFER_SIZE] __attribute__((aligned(4)));
volatile uint16_t audio_head = 0;
volatile uint16_t audio_tail = 0;

#define GPSD (*(volatile uint32_t *)0x60000368)

void IRAM_ATTR audio_timer_isr() {
  if (audio_head != audio_tail) {
    uint8_t sample = audio_buffer[audio_tail];
    audio_tail = (audio_tail + 1) % AUDIO_BUFFER_SIZE;
    
    // Мгновенная 32-битная запись в регистр прямо из IRAM (без прыжков во Flash)
    GPSD = (GPSD & 0xFFFFFF00) | sample;
  } else {
    // Тишина
    GPSD = (GPSD & 0xFFFFFF00) | 128; 
  }
}

void setup_audio() {
  // Несущая частота Sigma-Delta ДОЛЖНА БЫТЬ ультразвуковой, чтобы мы ее не слышали (312.5 кГц)
  sigmaDeltaSetup(0, 312500);
  sigmaDeltaAttachPin(SOUNDPIN);
  sigmaDeltaEnable();
  
  // А вот таймер прерываний тикает с частотой дискретизации звука
  timer1_attachInterrupt(audio_timer_isr);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
  timer1_write(80 * 1000000 / AUDIO_SAMPLE_RATE); 
}

extern "C" void push_audio_sample(uint8_t sample) {
  uint16_t next_head = (audio_head + 1) % AUDIO_BUFFER_SIZE;
  if (next_head != audio_tail) {
    audio_buffer[audio_head] = sample;
    audio_head = next_head;
  }
}


// Глобальный флаг режима экрана 
 uint8_t scale_fullscreen = 1;

// ==========================================
// Ввод (Маппинг кнопок ESPboy на Game Gear)
// ==========================================
void update_keys() {
  uint8_t keys = myESPboy.getKeys();
  
  input.pad[0] = 0;
  input.system = 0;

  // D-Pad
  if (keys & PAD_UP)    input.pad[0] |= INPUT_UP;
  if (keys & PAD_DOWN)  input.pad[0] |= INPUT_DOWN;
  if (keys & PAD_LEFT)  input.pad[0] |= INPUT_LEFT;
  if (keys & PAD_RIGHT) input.pad[0] |= INPUT_RIGHT;
  
  // Кнопки действий (A и B)
  if (keys & PAD_ACT)   input.pad[0] |= INPUT_BUTTON1;
  if (keys & PAD_ESC)   input.pad[0] |= INPUT_BUTTON2;

  // --- Защищенный автомат комбо LFT + RGT с подавлением утечек ---
  static uint8_t side_keys_timer = 0;
  static bool combo_handled = false;

  bool lft = (keys & PAD_LFT) != 0;
  bool rgt = (keys & PAD_RGT) != 0;

  if (lft && rgt) {
    // 1. Обе кнопки зажаты: переключаем режим строго один раз
    if (!combo_handled) {
      while(nbSPI_isBusy()); 
      scale_fullscreen = !scale_fullscreen; 
      combo_handled = true;
    }
    side_keys_timer = 0; // Блокируем одиночные таймеры
  } 
  else if (!lft && !rgt) {
    // 2. Обе кнопки полностью отпущены: сбрасываем защелку
    combo_handled = false;
    side_keys_timer = 0;
  } 
  else {
    // 3. Зажата только одна из боковых кнопок
    // Если мы только что вышли из комбо, игнорируем "хвост" отпускания
    if (!combo_handled) {
      // Фильтр дребезга комбо: ждем 2 кадра (~60 мс) перед отправкой Start/Pause
      if (side_keys_timer < 2) {
        side_keys_timer++;
      } else {
        if (rgt) input.system |= INPUT_START;
        // На Game Gear сигнал PAUSE (NMI) заблокирован во избежание сбоев ядра
        if (lft && !IS_GG) input.system |= INPUT_PAUSE;
      }
    }
  }
}

// ==========================================
// Инициализация Arduino
// ==========================================
void setup() {
  //Serial.begin(115200);
  myESPboy.begin("SEGA GameGear");
  myESPboy.tft.fillScreen(TFT_BLACK);

  //Serial.println();
  //Serial.println(ESP.getFreeHeap());
  
  // Включаем звук
  setup_audio();

  // Настройка базовых параметров эмулятора
  system_reset_config();
  option.console = CONSOLE_GG;
  sms.console = CONSOLE_GG;
  option.sndrate = AUDIO_SAMPLE_RATE; // 11025 Гц
  
  cart.rom = (uint8_t*)gg_rom_data;
  cart.size = gg_rom_size;
  cart.pages = (cart.size + 0x3FFF) / 0x4000;
  cart.mapper = MAPPER_SEGA;
  
  system_poweron();

  for (int i = 0; i < (AUDIO_BUFFER_SIZE / 2); i++) {
    push_audio_sample(128);
  }
}


// Функция для безопасного подсчета количества сэмплов в буфере
uint16_t get_audio_buffer_length() {
  uint16_t h = audio_head;
  uint16_t t = audio_tail;
  if (h >= t) return h - t;
  return AUDIO_BUFFER_SIZE - t + h;
}

// ==========================================
// Главный цикл Arduino
// ==========================================
void loop() {
 static uint8_t skip = 0;
  if (get_audio_buffer_length() < (AUDIO_BUFFER_SIZE / 2)) {
    if (!skip) update_keys();
    system_frame(skip);
    skip ^= 1;
  }
}
