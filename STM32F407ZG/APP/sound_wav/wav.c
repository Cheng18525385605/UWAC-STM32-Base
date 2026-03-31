#include "wav.h"
#include "adc.h"
#include "malloc.h"
#include "uart_cmd_process.h"
#include "ff.h"

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
    
    // 1. 重置状态，别的文件中定义的adc.c中
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

//得到path路径下,目标文件的总个数
//path:路径		    
//返回值:总有效文件数
u16 audio_get_tnum(u8 *path)
{	  
	u8 res;
	u16 rval=0;
 	DIR tdir;	 		//临时目录
	FILINFO* tfileinfo;	//临时文件信息	 	
	tfileinfo=(FILINFO*)mymalloc(SRAMIN,sizeof(FILINFO));//申请内存
    res=f_opendir(&tdir,(const TCHAR*)path); //打开目录 
	if(res==FR_OK&&tfileinfo)
	{
		while(1)//查询总的有效文件数
		{
	        res=f_readdir(&tdir,tfileinfo);       			//读取目录下的一个文件
	        if(res!=FR_OK||tfileinfo->fname[0]==0)break;	//错误了/到末尾了,退出	 		 
			res=f_typetell((u8*)tfileinfo->fname);	
			if((res&0XF0)==0X40)//取高四位,看看是不是音乐文件	
			{
				rval++;//有效文件数增加1
			}	    
		}  
	}  
	myfree(SRAMIN,tfileinfo);//释放内存
	return rval;
}

// WAV解析初始化（精简版：只解析标准WAV文件头）
// fname: 文件路径+文件名
// wav_header: 用来存放解析出的WAV头信息
// 返回值：0=成功，1=打开失败，2=非WAV文件
u8 wav_decode_init(u8 *fname, WAV_Header *wav_header)
{
    FIL file;
    FRESULT res;
    UINT br;  // 实际读取字节数

    // 1. 打开WAV文件
    res = f_open(&file, (TCHAR *)fname, FA_READ);
    if (res != FR_OK) return 1; // 打开文件失败

    // 2. 直接读取 44 字节标准 WAV 头（你给的结构体正好44字节）
    res = f_read(&file, wav_header, sizeof(WAV_Header), &br);
    
    // 3. 关闭文件（我们只需要头，读完立刻关！）
    f_close(&file);

    if (res != FR_OK || br < sizeof(WAV_Header)) return 2;

    // 4. 校验是不是 RIFF + WAVE
    if (memcmp(wav_header->chunkID, "RIFF", 4) != 0 ||
        memcmp(wav_header->format, "WAVE", 4) != 0)
    {
        return 2; // 不是标准 WAV 文件
    }

    // 5. 校验 fmt 和 data 标记
    if (memcmp(wav_header->subchunk1ID, "fmt ", 4) != 0 ||
        memcmp(wav_header->subchunk2ID, "data", 4) != 0)
    {
        return 2;
    }

    return 0; // 解析成功！
}

// u8 start_wav_pwm((char *)pname)
// {
//     WAV_Header wav;
//     u8 res = wav_decode_init(pname, &wav);
//    /* TIM Configuration */
//   TIM_Config();

//   /* TIM1 DMA Transfer example -------------------------------------------------
  
//   TIM1 input clock (TIM1CLK) is set to 2 * APB2 clock (PCLK2), since APB2 
//   prescaler is different from 1.   
//     TIM1CLK = 2 * PCLK2  
//     PCLK2 = HCLK / 2 
//     => TIM1CLK = 2 * (HCLK / 2) = HCLK = SystemCoreClock
  
//   TIM1CLK = SystemCoreClock, Prescaler = 0, TIM1 counter clock = SystemCoreClock
//   SystemCoreClock is set to 168 MHz for STM32F4xx devices.

//   The objective is to configure TIM1 channel 3 to generate complementary PWM
//   signal with a frequency equal to 17.57 KHz:
//      - TIM1_Period = (SystemCoreClock / 17570) - 1
//   and a variable duty cycle that is changed by the DMA after a specific number of
//   Update DMA request.

//   The number of this repetitive requests is defined by the TIM1 Repetion counter,
//   each 3 Update Requests, the TIM1 Channel 3 Duty Cycle changes to the next new 
//   value defined by the aSRC_Buffer.
  
//   Note: 
//     SystemCoreClock variable holds HCLK frequency and is defined in system_stm32f4xx.c file.
//     Each time the core clock (HCLK) changes, user had to call SystemCoreClockUpdate()
//     function to update SystemCoreClock variable value. Otherwise, any configuration
//     based on this variable will be incorrect.  
//   -----------------------------------------------------------------------------*/
  
//   /* Compute the value to be set in ARR register to generate signal frequency at 17.57 Khz */
//   uhTimerPeriod = (SystemCoreClock / 17570 ) - 1;
//   /* Compute CCR1 value to generate a duty cycle at 50% */
//   aSRC_Buffer[0] = (uint16_t) (((uint32_t) 5 * (uhTimerPeriod - 1)) / 10);
//   /* Compute CCR1 value to generate a duty cycle at 37.5% */
//   aSRC_Buffer[1] = (uint16_t) (((uint32_t) 375 * (uhTimerPeriod - 1)) / 1000);
//   /* Compute CCR1 value to generate a duty cycle at 25% */
//   aSRC_Buffer[2] = (uint16_t) (((uint32_t) 25 * (uhTimerPeriod - 1)) / 100);

//   /* TIM1 Peripheral Configuration -------------------------------------------*/
//   /* TIM1 clock enable */
//   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

//   /* Time Base configuration */
//   TIM_TimeBaseStructure.TIM_Prescaler = 0;
//   TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
//   TIM_TimeBaseStructure.TIM_Period = uhTimerPeriod;
//   TIM_TimeBaseStructure.TIM_ClockDivision = 0;
//   TIM_TimeBaseStructure.TIM_RepetitionCounter = 3;

//   TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

//   /* Channel 3 Configuration in PWM mode */
//   TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
//   TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//   TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
//   TIM_OCInitStructure.TIM_Pulse = aSRC_Buffer[0];
//   TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
//   TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
//   TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
//   TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

//   TIM_OC3Init(TIM1, &TIM_OCInitStructure);

//   /* Enable preload feature */
//   TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
  
//   /* TIM1 counter enable */
//   TIM_Cmd(TIM1, ENABLE);
  
//   /* DMA enable*/
//   DMA_Cmd(DMA2_Stream6, ENABLE);
  
//   /* TIM1 Update DMA Request enable */
//   TIM_DMACmd(TIM1, TIM_DMA_CC3, ENABLE);

//   /* Main Output Enable */
//   TIM_CtrlPWMOutputs(TIM1, ENABLE);

//   while (1)
//   {
//   }
// }

// /**
//   * @brief  Configure the TIM1 Pins.
//   * @param  None
//   * @retval None
//   */
// static void TIM_Config(void)
// {
//   GPIO_InitTypeDef GPIO_InitStructure;
//   DMA_InitTypeDef DMA_InitStructure;
  
//   /* GPIOA and GPIOB clock enable */
//   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB, ENABLE);

//   /* GPIOA Configuration: Channel 3 as alternate function push-pull */
//   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 ;
//   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
//   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
//   GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
//   GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
//   GPIO_Init(GPIOA, &GPIO_InitStructure); 
//   GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_TIM1);

//   /* GPIOB Configuration: Channel 3N as alternate function push-pull */
//   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
//   GPIO_Init(GPIOB, &GPIO_InitStructure);
//   GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_TIM1);

//   /* DMA clock enable */
//   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2 , ENABLE);

//   DMA_DeInit(DMA2_Stream6);
//   DMA_InitStructure.DMA_Channel = DMA_Channel_6;  
//   DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(TIM1_CCR3_ADDRESS) ;
//   DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)aSRC_Buffer;
//   DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
//   DMA_InitStructure.DMA_BufferSize = 3;
//   DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
//   DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
//   DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
//   DMA_InitStructure.DMA_MemoryDataSize = DMA_PeripheralDataSize_HalfWord;
//   DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
//   DMA_InitStructure.DMA_Priority = DMA_Priority_High;
//   DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
//   DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
//   DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
//   DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

//   DMA_Init(DMA2_Stream6, &DMA_InitStructure);
// }




