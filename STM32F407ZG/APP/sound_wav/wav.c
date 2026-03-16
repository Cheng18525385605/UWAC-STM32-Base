#include "wav.h"
#include "adc.h"
#include "malloc.h"
#include "uart_cmd_process.h"

uint32_t total_adc_samples;
uint8_t adc_record_finish  ;

// 全局变量
volatile uint32_t sample_counter = 0;
//uint16_t audio_buffer[BUFFER_SIZE];


/**
 * @brief  将ADC12位无符号数据转换为WAV的16位有符号PCM数据
 * @param  adc_data: 原始ADC数据缓冲区
 * @param  pcm_data: 输出PCM数据缓冲区
 * @param  num_samples: 转换的样本数
 */
void adc_to_pcm16(uint16_t *adc_data, int16_t *pcm_data, uint32_t num_samples)
{
    for(uint32_t i = 0; i < num_samples; i++)
    {
        // 12位ADC(0~4095) → 16位有符号(-32768~32767)
        // 步骤1：偏移到中心（2048为0点）；步骤2：缩放至16位范围
        int32_t temp = (int32_t)adc_data[i] - 2048;  // 转为-2048~2047
        temp = temp * 16;                            // 缩放至-32768~32767（可调整缩放系数）
        pcm_data[i] = (int16_t)temp;
    }
}

///**
//  * @brief  生成10kHz正弦波信号
//  * @param  buffer: 输出缓冲区
//  * @param  num_samples: 要生成的采样点数
//  * @param  start_sample: 起始采样点
//  * @retval 无
//  */
//void generate_10khz_sine_wave(int16_t *buffer, uint32_t num_samples, uint32_t start_sample)
//{
//    uint32_t i;
//    float angle;
//    
//    for(i = 0; i < num_samples; i++)
//    {
//        // 计算当前采样点的相位
//        angle = 2.0f * PI * FREQUENCY * (start_sample + i) / SAMPLE_RATE;
//        
//        // 生成正弦波值 (-32767 到 32767)
//        buffer[i] = (int16_t)(AMPLITUDE * sinf(angle));
//    }
//}

/**
  * @brief  创建WAV文件头
  * @param  header: WAV头结构体指针
  * @param  sample_rate: 采样率
  * @param  bits_per_sample: 位深度
  * @param  channels: 声道数
  * @param  data_size: 音频数据大小（字节）
  * @retval 无
  */
void create_wav_header(WAV_Header *header, uint32_t sample_rate, 
                      uint16_t bits_per_sample, uint16_t channels, 
                      uint32_t data_size)
{
    // RIFF块
    memcpy(header->chunkID, "RIFF", 4);
    header->chunkSize = data_size + 36;  // 总文件大小-8
    memcpy(header->format, "WAVE", 4);
    
    // fmt子块
    memcpy(header->subchunk1ID, "fmt ", 4);
    header->subchunk1Size = 16;
    header->audioFormat = 1;  // PCM格式
    header->numChannels = channels;
    header->sampleRate = sample_rate;
    header->byteRate = sample_rate * channels * bits_per_sample / 8;
    header->blockAlign = channels * bits_per_sample / 8;
    header->bitsPerSample = bits_per_sample;
    
    // data子块
    memcpy(header->subchunk2ID, "data", 4);
    header->subchunk2Size = data_size;
}

/**
  * @brief  写入WAV文件到SD卡
  * @param  filename: 文件名
  * @retval FRESULT: FATFS操作结果
  */
/*
FRESULT write_wav_to_sd(const char *filename)
{
    FRESULT fr;
    FIL file;
    UINT bytes_written;
    uint32_t total_bytes = 0;
    uint32_t samples_written = 0;
    
    // 1. 创建文件
    fr = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK)
    {
        printf("Failed to create file: %d\n", fr);
        return fr;
    }
    
    // 2. 先写入一个空的WAV文件头
    WAV_Header wav_header;
    memset(&wav_header, 0, sizeof(WAV_Header));
    fr = f_write(&file, &wav_header, sizeof(WAV_Header), &bytes_written);
    if(fr != FR_OK || bytes_written != sizeof(WAV_Header))
    {
        f_close(&file);
        return FR_DISK_ERR;
    }
    
    total_bytes += bytes_written;
    
    // 3. 生成并写入音频数据
    LCD_ShowString(10, 140, tftlcd_data.width, tftlcd_data.height, 16, "Generating WAV...");
    printf("Generating 10kHz sine wave...\n");
    
    while(samples_written < TOTAL_SAMPLES)
    {
        uint32_t samples_to_generate = BUFFER_SIZE;
        
        // 最后一次可能不满一个缓冲区
        if(samples_written + samples_to_generate > TOTAL_SAMPLES)
        {
            samples_to_generate = TOTAL_SAMPLES - samples_written;
        }
        
        // 生成10kHz正弦波
        generate_10khz_sine_wave((int16_t *)audio_buffer, samples_to_generate, samples_written);
        
        // 写入到文件
        uint32_t bytes_to_write = samples_to_generate * 2;  // 16位 = 2字节
        fr = f_write(&file, audio_buffer, bytes_to_write, &bytes_written);
        
        if(fr != FR_OK || bytes_written != bytes_to_write)
        {
            printf("Write error at sample %du\n", samples_written);
            f_close(&file);
            return FR_DISK_ERR;
        }
        
        total_bytes += bytes_written;
        samples_written += samples_to_generate;
        
        // 显示进度
        if((samples_written % (SAMPLE_RATE/2)) == 0)  // 每0.5秒更新一次
        {
            uint8_t percent = (samples_written * 100) / TOTAL_SAMPLES;
            printf("Progress: %d%%\n", percent);
            LCD_ShowNum(10+8 * 10, 160, percent, 3, 16);
            LCD_ShowString(10+8 * 13, 160, tftlcd_data.width, tftlcd_data.height, 16, "%");
        }
        
        // LED闪烁指示进度
        LED1 = !LED1;
    }
    
    // 4. 回到文件开头，写入正确的WAV头
    f_lseek(&file, 0);
    create_wav_header(&wav_header, SAMPLE_RATE, 16, 1, TOTAL_SAMPLES * 2);
    
    fr = f_write(&file, &wav_header, sizeof(WAV_Header), &bytes_written);
    if(fr != FR_OK || bytes_written != sizeof(WAV_Header))
    {
        f_close(&file);
        return FR_DISK_ERR;
    }
    
    // 5. 关闭文件
    fr = f_close(&file);
    
    if(fr == FR_OK)
    {
        printf("WAV file created successfully!\n");
        printf("File size: %lu bytes\n", total_bytes);
        printf("Duration: %d seconds\n", DURATION);
        printf("Sample rate: %d Hz\n", SAMPLE_RATE);
        printf("Frequency: %d Hz\n", FREQUENCY);
        
        LCD_ShowString(10, 140, tftlcd_data.width, tftlcd_data.height, 16, "WAV file created!   ");
        LCD_ShowString(10, 160, tftlcd_data.width, tftlcd_data.height, 16, "Size:           B  ");
        LCD_ShowNum(10+8 * 6, 160, total_bytes, 8, 16);
    }
    
    return fr;
}
*/
/**
 * @brief  将ADC采样数据写入SD卡为WAV文件
 * @param  filename: 保存的文件名（如"ADC_REC.WAV"）
 * @retval FRESULT: FATFS操作结果
 */
FRESULT adc_save_to_wav(const char *filename)
{
    FRESULT fr;
    FIL file;
    UINT bytes_written;
    uint32_t total_bytes = 0;
    uint32_t samples_written = 0;
    
    // 【修改1】改为指针，不再是在栈上开辟大数组
    int16_t *pcm_buffer; 
    
    // 【修改2】动态申请内存 (SRAMIN=0, 大小=点数*2字节)
    pcm_buffer = (int16_t*)mymalloc(SRAMIN, ADC1_DMA_Size * 2);
    
    // 【修改3】检查内存是否申请成功
    if(pcm_buffer == NULL)
    {
        printf("Malloc pcm_buffer failed!\r\n");
        return FR_NOT_ENOUGH_CORE; // 内存不足
    }
    
    // 1. 重置状态
    total_adc_samples = 0;
    adc_record_finish = 0;
    curr_write_buf = 0xFF;
    
    // 2. 创建并打开WAV文件
    fr = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK)
    {
        printf("Failed to create WAV file: %d\n", fr);
        myfree(SRAMIN, pcm_buffer); // 【修改4】出错返回前必须释放
        return fr;
    }
		
		DWORD pre_alloc_size = ADC_SAMPLE_RATE * my_cmd_data.time * 2 + 1024; 
    
    // 移动指针到预计大小处
    fr = f_lseek(&file, pre_alloc_size);
    if (fr == FR_OK) {
        f_lseek(&file, 0); // 分配成功，指针移回开头准备写入
    }
    
    // 3. 先写入空的WAV头部
    WAV_Header wav_header;
    memset(&wav_header, 0, sizeof(WAV_Header));
    fr = f_write(&file, &wav_header, sizeof(WAV_Header), &bytes_written);
    if(fr != FR_OK || bytes_written != sizeof(WAV_Header))
    {
        f_close(&file);
        printf("Write empty header failed!\n");
        myfree(SRAMIN, pcm_buffer); // 【修改4】出错返回前必须释放
        return FR_DISK_ERR;
    }
    total_bytes += bytes_written;
    
    LCD_ShowString(10, 140, tftlcd_data.width, tftlcd_data.height, 16, "Recording ADC...");
    printf("Recording ADC data to WAV...\n");
    
    // 4. 循环写入
    while(samples_written < TOTAL_ADC_SAMPLES && !adc_record_finish)
    {
        if(curr_write_buf != 0xFF)
        {
            uint8_t buf_idx = curr_write_buf;
            curr_write_buf = 0xFF;
            
            uint32_t samples_to_write = ADC1_DMA_Size;
            if(samples_written + samples_to_write > TOTAL_ADC_SAMPLES)
            {
                samples_to_write = TOTAL_ADC_SAMPLES - samples_written;
            }
            
            // 使用动态申请的 pcm_buffer
            adc_to_pcm16(adc_buf[buf_idx].data, pcm_buffer, samples_to_write);
            
            uint32_t bytes_to_write = samples_to_write * 2;
            fr = f_write(&file, pcm_buffer, bytes_to_write, &bytes_written);
            
            if(fr != FR_OK || bytes_written != bytes_to_write)
            {
                printf("Write ADC buf%d failed: %d\n", buf_idx, fr);
                adc_record_finish = 1;
                break;
            }
            
            total_bytes += bytes_written;
            samples_written += samples_to_write;
            total_adc_samples += samples_to_write;
            
            if((samples_written % (ADC_SAMPLE_RATE/2)) == 0)
            {
                uint8_t percent = (samples_written * 100) / TOTAL_ADC_SAMPLES;
//                printf("Progress: %d%%\n", percent);
//                LCD_ShowNum(10+8*10, 160, percent, 3, 16);
//                LCD_ShowString(10+8*13, 160, tftlcd_data.width, tftlcd_data.height, 16, "%");
            }
            LED1 = !LED1;
        }
    }
    
    // 5. 停止采集
    TIM_Cmd(TIM3, DISABLE);
    DMA_Cmd(DMA2_Stream0, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    adc_record_finish = 1;
    
    // 6. 补全头部
    f_lseek(&file, 0);
    create_wav_header(&wav_header, ADC_SAMPLE_RATE, 16, 1, samples_written * 2);
    f_write(&file, &wav_header, sizeof(WAV_Header), &bytes_written);
		
		f_lseek(&file, total_bytes); // 移动到实际写入的数据末尾
    f_truncate(&file);           // 截断文件
    
    // 7. 关闭文件
    f_close(&file);
    
    // 【修改5】最后一定要释放内存！
    myfree(SRAMIN, pcm_buffer);

    if(fr == FR_OK)
    {
        printf("ADC WAV saved successfully!\n");
        printf("File size: %lu bytes\n", total_bytes);
        printf("Record duration: %.2f s\n", (float)samples_written/ADC_SAMPLE_RATE);
        LCD_ShowString(10, 140, tftlcd_data.width, tftlcd_data.height, 16, "WAV saved!");
        LCD_ShowString(10, 160, tftlcd_data.width, tftlcd_data.height, 16, "Size:           B");
        LCD_ShowNum(10+8*6, 160, total_bytes, 8, 16);
    }
    else
    {
        LCD_ShowString(10, 140, tftlcd_data.width, tftlcd_data.height, 16, "WAV save failed!");
    }
    
    return fr;
}








