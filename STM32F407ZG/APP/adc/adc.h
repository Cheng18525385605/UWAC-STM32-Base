#ifndef _adc_H
#define _adc_H

#include "system.h"

#define ADC1_DR_ADDRESS  ((uint32_t)0x4001204C)  //ADC1 DR

#define ADC1_DMA_Size  8192 //采样点数
//#define UART_DMA_BIN_Size ADC1_DMA_Size*2
//#define UART_DMA_STR_size ADC1_DMA_Size*25

typedef enum {
    BUF_IDLE,        // 空闲（可分配给ADC采集）
    BUF_COLLECTING,  // 正在ADC采集
    BUF_READY,       // 采集完成，待SD写入
    BUF_WRITING      // 正在SDIO-DMA写入
} BufState;

// 双缓冲区结构体（统一管理）
typedef struct {
    u16 data[ADC1_DMA_Size];  // 4KB缓冲区（匹配SD扇区、ADC分块）
    BufState state;  // 缓冲区状态
    u32 len;         // 该缓冲区的有效数据长度
} ADC_Buf_t;

extern u16 ADC1_ConvertedValue[ ADC1_DMA_Size ];
//extern u8 UART_DMA_BIN_BUFF[UART_DMA_BIN_Size];
//extern u8 UART_DMA_STR_BUFF[UART_DMA_STR_size];
extern  u16 tc_counter;
extern  u16 totol_sample_num;
extern u8 curr_collect_buf ;  // 当前用于采集的缓冲区索引（0/BufA，1/BufB）
extern u8 curr_write_buf ;    // 当前用于写入的缓冲区索引（初始错开）
extern u8 collect_finish ;    // 整体采集完成标志
extern ADC_Buf_t adc_buf[2];

void ADCx_Init(void);
u16 Get_ADC_Value(u8 ch,u8 times);
void TIM3_Config( u32 Fre );
void ADC_DMA_Trig( u16 Size );
void ADC_Config(void);
void DMA2_Stream0_IRQHandler( void );
void ADC_U16_To_UART_BinBuff(u16 *adc_buf, u8 *uart_bin_buf, u16 len);
u16 ADC_U16_To_UART_StrBuff(u16 *adc_buf, u8 *uart_str_buf, u16 len);

#endif
