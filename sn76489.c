#include "shared.h"
#include <limits.h>

#pragma GCC optimize ("O3")

#define NoiseInitialState   0x8000
#define PSG_CUTOFF          0x6

static const int PSGVolumeValues[2][16] = {
    {892,892,892,760,623,497,404,323,257,198,159,123,96,75,60,0}, 
    {892,774,669,575,492,417,351,292,239,192,150,113,80,50,24,0}
};

static SN76489_Context SN76489;

void SN76489_Init(int which, int PSGClockValue, int SamplingRate)
{
    // Конвертация в fixed-point (умножаем на 65536)
    SN76489.dClock = (uint32_t)(((float)PSGClockValue / 16.0f / (float)SamplingRate) * 65536.0f);
    SN76489.Mute = MUTE_ALLON;
    SN76489.BoostNoise = BOOST_ON;
    SN76489.VolumeArray = VOL_FULL;
    SN76489.WhiteNoiseFeedback = FB_SEGAVDP;
    SN76489_Reset(0);
}

void SN76489_Reset(int which)
{
    SN76489.PSGStereo = 0xFF;
    for(int i = 0; i <= 3; i++) {
        SN76489.Registers[2*i] = 1;
        SN76489.Registers[2*i+1] = 0xf;
        SN76489.ToneFreqVals[i] = 0;
        SN76489.ToneFreqPos[i] = 1;
        SN76489.IntermediatePos[i] = LONG_MIN;
    }
    SN76489.NoiseFreq = 0x10;
    SN76489.LatchedRegister=0;
    SN76489.NoiseShiftRegister=NoiseInitialState;
    SN76489.Clock=0;
}

void SN76489_Shutdown(void) {}

void SN76489_Write(int which, int data)
{
    if (data&0x80) {
        SN76489.LatchedRegister=((data>>4)&0x07);
        SN76489.Registers[SN76489.LatchedRegister] = 
            (SN76489.Registers[SN76489.LatchedRegister] & 0x3f0) | (data&0xf);
    } else {
        if (!(SN76489.LatchedRegister%2)&&(SN76489.LatchedRegister<5))
            SN76489.Registers[SN76489.LatchedRegister] = 
                (SN76489.Registers[SN76489.LatchedRegister] & 0x00f) | ((data&0x3f)<<4);
        else
            SN76489.Registers[SN76489.LatchedRegister]=data&0x0f;
    }
    switch (SN76489.LatchedRegister) {
    case 0: case 2: case 4:
        if (SN76489.Registers[SN76489.LatchedRegister]==0) 
            SN76489.Registers[SN76489.LatchedRegister]=1;
        break;
    case 6:
        SN76489.NoiseShiftRegister=NoiseInitialState;
        SN76489.NoiseFreq=0x10<<(SN76489.Registers[6]&0x3);
        break;
    }
}

void SN76489_GGStereoWrite(int which, int data)
{
    SN76489.PSGStereo=data;
}


void SN76489_Update(int which, int16_t *buffer, int length)
{
    int i, j;
    for(j = 0; j < length; j++)
    {
        for (i=0;i<=2;++i)
            if (SN76489.IntermediatePos[i]!=LONG_MIN)
                SN76489.Channels[i]=(SN76489.Mute >> i & 0x1)*PSGVolumeValues[SN76489.VolumeArray][SN76489.Registers[2*i+1]]*SN76489.IntermediatePos[i]/65536;
            else
                SN76489.Channels[i]=(SN76489.Mute >> i & 0x1)*PSGVolumeValues[SN76489.VolumeArray][SN76489.Registers[2*i+1]]*SN76489.ToneFreqPos[i];

        SN76489.Channels[3]=(short)((SN76489.Mute >> 3 & 0x1)*PSGVolumeValues[SN76489.VolumeArray][SN76489.Registers[7]]*(SN76489.NoiseShiftRegister & 0x1));

        if (SN76489.BoostNoise) SN76489.Channels[3]<<=1;

        buffer[j] = 0;
        for (i=0;i<=3;++i) {
            int left_on  = (SN76489.PSGStereo >> (i+4)) & 0x1;
            int right_on = (SN76489.PSGStereo >> i) & 0x1;
            
            if (left_on && right_on) {
                // Звук по центру - полная громкость
                buffer[j] += SN76489.Channels[i]; 
            } else if (left_on || right_on) {
                // Звук смещен - половинная громкость для моно
                buffer[j] += SN76489.Channels[i] / 2; 
            }
        }
        
        // Fixed point math
        SN76489.Clock += SN76489.dClock;
        SN76489.NumClocksForSample = SN76489.Clock >> 16;
        SN76489.Clock &= 0xFFFF; // Оставляем дробную часть

        for (i=0;i<=2;++i) SN76489.ToneFreqVals[i] -= SN76489.NumClocksForSample;

        if (SN76489.NoiseFreq==0x80) SN76489.ToneFreqVals[3]=SN76489.ToneFreqVals[2];
        else SN76489.ToneFreqVals[3]-=SN76489.NumClocksForSample;

        for (i=0;i<=2;++i) {
            if (SN76489.ToneFreqVals[i]<=0) {
                if (SN76489.Registers[i*2]>PSG_CUTOFF) {
                    SN76489.IntermediatePos[i]=LONG_MIN; 
                    SN76489.ToneFreqPos[i]=-SN76489.ToneFreqPos[i];
                } else {
                    SN76489.ToneFreqPos[i]=1;
                    SN76489.IntermediatePos[i]=LONG_MIN;
                }
                SN76489.ToneFreqVals[i]+=SN76489.Registers[i*2]*(SN76489.NumClocksForSample/SN76489.Registers[i*2]+1);
            } else SN76489.IntermediatePos[i]=LONG_MIN;
        }

        if (SN76489.ToneFreqVals[3]<=0) {
            SN76489.ToneFreqPos[3]=-SN76489.ToneFreqPos[3];
            if (SN76489.NoiseFreq!=0x80)
                SN76489.ToneFreqVals[3]+=SN76489.NoiseFreq*(SN76489.NumClocksForSample/SN76489.NoiseFreq+1);
            if (SN76489.ToneFreqPos[3]==1) {
                int Feedback;
                if (SN76489.Registers[6]&0x4) {
                    Feedback=((SN76489.NoiseShiftRegister&SN76489.WhiteNoiseFeedback) && ((SN76489.NoiseShiftRegister&SN76489.WhiteNoiseFeedback)^SN76489.WhiteNoiseFeedback));
                } else Feedback=SN76489.NoiseShiftRegister&1;

                SN76489.NoiseShiftRegister=(SN76489.NoiseShiftRegister>>1) | (Feedback<<15);
            }
        }
    }
}
