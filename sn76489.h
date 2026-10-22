#ifndef _SN76489_H_
#define _SN76489_H_

#define FB_SEGAVDP  0x0009
#define VOL_FULL    1
#define BOOST_ON    1
#define MUTE_ALLON  15

typedef struct
{
    int Mute;
    int BoostNoise;
    int VolumeArray;
    int WhiteNoiseFeedback;

    /* Изменено на fixed-point 16.16 для ускорения на ESP8266 */
    uint32_t Clock;  
    uint32_t dClock; 
    
    int PSGStereo;
    int NumClocksForSample;

    uint16_t Registers[8];
    int LatchedRegister;
    uint16_t NoiseShiftRegister;
    int16_t NoiseFreq;

    int16_t ToneFreqVals[4];
    int8_t ToneFreqPos[4];
    int16_t Channels[4];
    int32_t IntermediatePos[4];

} SN76489_Context;

void SN76489_Init(int which, int PSGClockValue, int SamplingRate);
void SN76489_Reset(int which);
void SN76489_Shutdown(void);
void SN76489_Write(int which, int data);
void SN76489_GGStereoWrite(int which, int data);
void SN76489_Update(int which, int16_t *buffer, int length);

#endif /* _SN76489_H_ */