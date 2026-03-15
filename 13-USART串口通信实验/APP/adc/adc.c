#include "adc.h"
#include "SysTick.h"
#include "stdio.h"
#include "string.h"
#include "stm32f4xx_dma.h"
#include "usart.h"	


 u16 tc_counter=0;
//u16 ADC1_ConvertedValue[ ADC1_DMA_Size ];


// 全局双缓冲区（核心）
ADC_Buf_t adc_buf[2] = {
    {.state = BUF_IDLE, .len = ADC1_DMA_Size},
    {.state = BUF_IDLE, .len = ADC1_DMA_Size}
};
u8 curr_collect_buf = 0;  // 当前用于采集的缓冲区索引（0/BufA，1/BufB）
u8 curr_write_buf = 1;    // 当前用于写入的缓冲区索引（初始错开）
u8 collect_finish = 0;    // 整体采集完成标志

/*
//u8 UART_DMA_BIN_BUFF[UART_DMA_BIN_Size];
//u8 UART_DMA_STR_BUFF[UART_DMA_STR_size];

//void ADC_U16_To_UART_BinBuff(u16 *adc_buf, u8 *uart_bin_buf, u16 len)
//{
//    u16 i;
//    // 判空保护，避免空指针操作
//    if(adc_buf == NULL || uart_bin_buf == NULL || len == 0)
//    {
//        return;
//    }
//    
//    // 循环拆分每个u16数据为2个u8字节
//    for(i = 0; i < len; i++)
//    {
//        uart_bin_buf[i * 2]     = (u8)(adc_buf[i] & 0xFF);        // 提取低8位，存入偶数索引
//        uart_bin_buf[i * 2 + 1] = (u8)(adc_buf[i] >> 8);           // 提取高8位，存入奇数索引
//    }
//}
//u16 ADC_U16_To_UART_StrBuff(u16 *adc_buf, u8 *uart_str_buf, u16 len)
//{
//    u16 i;
//    u32 str_index = 0;  // 字符串缓冲区索引
//    char temp_buf[25];  // 临时缓存单个采样值的字符串
//    
//    // 判空保护，避免空指针操作
//    if(adc_buf == NULL || uart_str_buf == NULL || len == 0)
//    {
//        return 0;
//    }
//    
//    // 清空字符串缓冲区（可选，避免残留数据）
//    memset(uart_str_buf, 0, sizeof(uart_str_buf));
//    
//    // 循环转换每个u16采样值为ASCII字符串
//    for(i = 0; i < len; i++)
//    {
//        // 格式化单个采样值：“第i个数据：adc_buf[i]\r\n”，存入临时缓冲区
//        // snprintf自动将数字转为ASCII字符，确保格式统一
//        snprintf(temp_buf, sizeof(temp_buf), "第%d个数据：%d\r\n", i+1, adc_buf[i]);
//        
//        // 将临时缓冲区的字符串拷贝到输出缓冲区，更新索引
//        for(u8 j = 0; j < strlen(temp_buf); j++)
//        {
//            uart_str_buf[str_index++] = (u8)temp_buf[j];
//        }
//    }
//    return (u16)str_index;  // 返回转换后字符串的总长度
//}*/

/*******************************************************************************
* 函 数 名         : ADCx_Init
* 函数功能		   : ADC初始化	
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void ADCx_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure; //定义结构体变量	
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef       ADC_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AN; //模拟输入模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_5;//管脚设置
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;//浮空
	GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化结构体
	
	//RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,ENABLE);	  //ADC1复位
	//RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,DISABLE);	//复位结束
	
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;//独立模式
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;//两个采样阶段之间的延迟5个时钟
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; //DMA失能
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;//预分频4分频。ADCCLK=PCLK2/4=84/4=21Mhz,ADC时钟最好不要超过36Mhz 
	ADC_CommonInit(&ADC_CommonInitStructure);//初始化
	
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;//12位模式
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;//非扫描模式	
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;//关闭连续转换
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;//禁止触发检测，使用软件触发
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//右对齐	
	ADC_InitStructure.ADC_NbrOfConversion = 1;//1个转换在规则序列中 也就是只转换规则序列1 
	ADC_Init(ADC1, &ADC_InitStructure);//ADC初始化
	
	ADC_Cmd(ADC1, ENABLE);//开启AD转换器
}

/*******************************************************************************
* 函 数 名         : Get_ADC_Value
* 函数功能		   : 获取通道ch的转换值，取times次,然后平均 	
* 输    入         : ch:通道编号
					 times:获取次数
* 输    出         : 通道ch的times次转换结果平均值
*******************************************************************************/
u16 Get_ADC_Value(u8 ch,u8 times)
{
	u32 temp_val=0;
	u8 t;
	//设置指定ADC的规则组通道，一个序列，采样时间
	ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_480Cycles);	//ADC1,ADC通道,480个周期,提高采样时间可以提高精确度			    
	
	for(t=0;t<times;t++)
	{
		ADC_SoftwareStartConv(ADC1);		//使能指定的ADC1的软件转换启动功能	
		while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC ));//等待转换结束
		temp_val+=ADC_GetConversionValue(ADC1);
		delay_ms(5);
	}
	return temp_val/times;
} 




void TIM3_Config( u32 Fre )
{
	TIM_TimeBaseInitTypeDef  	TIM_TimeBaseStructure;
	u32 MaxData;
	u16 div=1;
	
	while( (SystemCoreClock/2/Fre/div)>65535 )
	{
		div++;
	}
	MaxData =  SystemCoreClock/2/Fre/div - 1;
	
/*   时钟等配置 */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3 , ENABLE );			//开启时钟
	
	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = MaxData ;
	TIM_TimeBaseStructure.TIM_Prescaler = div-1;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit( TIM3 , &TIM_TimeBaseStructure );
	
	TIM_SelectOutputTrigger( TIM3 , TIM_TRGOSource_Update );
	TIM_ARRPreloadConfig( TIM3 , ENABLE );
	TIM_Cmd( TIM3 , DISABLE );							/* TIM3 enable counter */
}

void ADC_DMA_Trig( u16 Size )
{
	DMA2_Stream0->CR &= ~((uint32_t)DMA_SxCR_EN);
	DMA2_Stream0->NDTR = Size;             //设置数量
	DMA_ClearITPendingBit( DMA2_Stream0 ,DMA_IT_TCIF0|DMA_IT_DMEIF0|DMA_IT_TEIF0|DMA_IT_HTIF0|DMA_IT_TCIF0 );
	ADC1->SR = 0;
	DMA2_Stream0->CR |= (uint32_t)DMA_SxCR_EN;
}	
void ADC_Config(void)
{
	ADC_InitTypeDef       ADC_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	DMA_InitTypeDef       DMA_InitStructure;
	GPIO_InitTypeDef 		GPIO_InitStructure; //定义结构体变量	
	NVIC_InitTypeDef 	  NVIC_InitStructure;
	
	ADC_DeInit( );
	DMA_DeInit( DMA2_Stream0 );
	/* Enable ADCx, DMA and GPIO clocks ****************************************/ 
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_DMA2 , ENABLE ); 
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC1 , ENABLE );
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AN; //模拟输入模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_5;//管脚设置
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;//浮空
	GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化结构体
	
	/* Enable the DMA Stream IRQ Channel */
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);     
	
	/* DMA2 Stream0 channel0 configuration **************************************/
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;  
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC1_DR_ADDRESS;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)adc_buf[0].data;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = ADC1_DMA_Size;                      //DMA 传输数量
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;         
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init( DMA2_Stream0 , &DMA_InitStructure );
	DMA_Cmd( DMA2_Stream0 , DISABLE );
	
	// 单独配置双缓冲区（内存1地址 + 初始激活内存0）
	// 关键：配置内存1地址为adc_buf1，初始激活内存0（先采集buf0）
	DMA_DoubleBufferModeConfig(DMA2_Stream0, (uint32_t)adc_buf[1].data, DMA_Memory_0);
	// 使能双缓冲区模式（核心！不调用则仅单缓冲区）
	DMA_DoubleBufferModeCmd(DMA2_Stream0, ENABLE);
	
	DMA_ClearITPendingBit( DMA2_Stream0 ,DMA_IT_TCIF0 );
	DMA_ITConfig( DMA2_Stream0 , DMA_IT_TC , ENABLE );

	/* ADC Common Init **********************************************************/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit( &ADC_CommonInitStructure );

	/* ADC1 Init ****************************************************************/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;//配置外部触发
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init( ADC1, &ADC_InitStructure );

	/* ADC3 regular channel7 configuration **************************************/
	ADC_RegularChannelConfig( ADC1 , ADC_Channel_5 , 1, ADC_SampleTime_3Cycles);  //PA2
	ADC_DMARequestAfterLastTransferCmd( ADC1 , ENABLE );
	
	DMA_Cmd( DMA2_Stream0 , ENABLE );
	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);
}

void DMA2_Stream0_IRQHandler( void )
{	
	if(DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0))
	{
			//DMA_Cmd(DMA2_Stream0,DISABLE);
			DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
			//查询当前使用的缓冲区（0=buf0，1=buf1）
      curr_collect_buf = DMA_GetCurrentMemoryTarget(DMA2_Stream0);
			// 标记当前采集缓冲区为“待写入”（禁止覆盖）
			curr_write_buf=(curr_collect_buf == 0) ? 1 : 0;
			tc_counter++;
			//printf("[TC] 触发次数: %d  现在的采样数%d\n", tc_counter,totol_sample_num+=ADC1_DMA_Size);
			//DMA_Cmd(DMA2_Stream0,ENABLE);
	}
}


