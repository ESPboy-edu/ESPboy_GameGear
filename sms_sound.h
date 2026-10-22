#ifndef _SMS_SOUND_H_
#define _SMS_SOUND_H_

#include "sn76489.h"

// Z80 вызывает эти функции при записи в порт звука (0x40 - 0x41 или 0x06 в GG)
static inline void psg_write(uint8 data) {
    SN76489_Write(0, data);
}

static inline void psg_stereo_w(uint8 data) {
    SN76489_GGStereoWrite(0, data);
}

static inline void sound_init(void) {
    SN76489_Init(0, 3579545, 8000);
}

static inline void sms_sound_reset(void) {
    SN76489_Reset(0);
}

static inline void sound_update(int line) {
    // Пусто. Мы генерируем звук батчами в конце кадра внутри system.c
}

static inline void sound_shutdown(void) {
    SN76489_Shutdown();
}

#endif /* _SMS_SOUND_H_ */