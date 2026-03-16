#ifndef _wav_H
#define _wav_H

#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "tftlcd.h" 
#include "malloc.h" 
#include "sdio_sdcard.h" 
#include "flash.h"
#include "ff.h" 
#include "fatfs_app.h"
#include "math.h"
#include <string.h>
#include <stdio.h>
#include "uart_cmd_process.h"

// WAV文件头结构体
#pragma pack(push, 1)  // 1字节对齐
typedef struct {
    // RIFF块
    char     chunkID[4];      // "RIFF"
    uint32_t chunkSize;       // 文件总大小-8
    char     format[4];       // "WAVE"
    
    // fmt子块
    char     subchunk1ID[4];  // "fmt "
    uint32_t subchunk1Size;   // 16
    uint16_t audioFormat;     // 1=PCM
    uint16_t numChannels;     // 1=单声道
    uint32_t sampleRate;      // 采样率
    uint32_t byteRate;        // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;      // numChannels * bitsPerSample/8
    uint16_t bitsPerSample;   // 16
    
    // data子块
    char     subchunk2ID[4];  // "data"
    uint32_t subchunk2Size;   // 音频数据大小
} WAV_Header;
#pragma pack(pop)

// 全局变量
//#define SAMPLE_RATE     8     // 采样率 48kHz
#define FREQUENCY       10000     // 信号频率 10kHz
#define DURATION        1         // 录音时长 单位秒
#define AMPLITUDE       30000     // 振幅 (16位有符号，最大32767)
#define PI              3.14159265358979323846f

// 计算总采样点数
#define TOTAL_SAMPLES   (SAMPLE_RATE * DURATION)

// 缓冲区大小
#define BUFFER_SIZE     1024      // 每次处理的采样点数
#define BUFFER_BYTES    (BUFFER_SIZE * 2)  // 16位 = 2字节

extern uint32_t total_adc_samples ; // 已采集的总样本数
extern uint8_t adc_record_finish  ;           // 录音完成标志


#define ADC_SAMPLE_RATE   600000       // ADC采样率（需和TIM3触发频率一致，匹配WAV的48kHz）
#define ADC_12BIT_MAX     4095        // 12位ADC最大值
#define ADC_RECORD_DURATION (my_cmd_data.time +1) // 录音时长（复用PWM的时间参数）
#define TOTAL_ADC_SAMPLES (ADC_SAMPLE_RATE * ADC_RECORD_DURATION) // 总采样点数

void generate_10khz_sine_wave(int16_t *buffer, uint32_t num_samples, uint32_t start_sample);
void create_wav_header(WAV_Header *header, uint32_t sample_rate, 
                      uint16_t bits_per_sample, uint16_t channels, 
                      uint32_t data_size);
FRESULT write_wav_to_sd(const char *filename);
FRESULT adc_save_to_wav(const char *filename);



#endif


