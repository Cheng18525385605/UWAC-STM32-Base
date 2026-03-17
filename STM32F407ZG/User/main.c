/*******************************************************************************
*
*                 		       普中科技
--------------------------------------------------------------------------------
* 实 验 名		 : USART串口通信实验
* 实验说明       :
* 连接方式       :
* 注    意		 : USART驱动程序在usart.c内
*******************************************************************************/

#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "uart_cmd_process.h"
#include "adc.h"
#include "dma.h"
#include "dma.h"
#include "tftlcd.h"
#include "flash.h"
#include "malloc.h"
#include "wav.h"

/*******************************************************************************
 * 函 数 名         : main
 * 函数功能		   : 主函数
 * 输    入         : 无
 * 输    出         : 无
 *******************************************************************************/
int main()
{
	u8 i = 0;
	u8 res = 0;
	u32 total, free;

	SysTick_Init(168);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 中断优先级分组 分2组
	USART1_Init(115200);
	//	USART2_Init(115200);
	LED_Init();
	// 采样率*（信号时间+1）=单缓冲区大小*触发次数，时间加一因为信号前后一共空出1s
	TIM3_Config(ADC_SAMPLE_RATE); // 参数为频率hz，adc的采样触发频率

	ADC_Config();
	// DMAx_Init(DMA1_Stream6,DMA_Channel_4,(u32)&USART2->DR,(u32)UART_DMA_STR_BUFF,UART_DMA_STR_size);
	TFTLCD_Init();			// LCD初始化
	EN25QXX_Init();		// 初始化EN25Q128
	my_mem_init(SRAMIN); // 初始化内部内存池

	FRONT_COLOR = RED; // 设置字体为红色
	LCD_ShowString(10, 10, tftlcd_data.width, tftlcd_data.height, 16, "PRECHIN STM32F4");
	LCD_ShowString(10, 30, tftlcd_data.width, tftlcd_data.height, 16, "Fatfs Test");
	LCD_ShowString(10, 50, tftlcd_data.width, tftlcd_data.height, 16, "www.prechin.net");

	while (SD_Init()) // 检测不到SD卡
	{
		LCD_ShowString(10, 100, tftlcd_data.width, tftlcd_data.height, 16, "SD Card Error!");
		printf("SD Card Error!\r\n");
		delay_ms(500);
	}

	FRONT_COLOR = BLUE; // 设置字体为蓝色
	// 检测SD卡成功
	printf("SD Card OK!\r\n");
	LCD_ShowString(10, 100, tftlcd_data.width, tftlcd_data.height, 16, "SD Card OK    ");

	FATFS_Init();						 // 为fatfs相关变量申请内存
	f_mount(fs[0], "0:", 1);		 // 挂载SD卡
	res = f_mount(fs[1], "1:", 1); // 挂载FLASH.
	if (res == 0X0D)					 // FLASH磁盘,FAT文件系统错误,重新格式化FLASH
	{
		LCD_ShowString(10, 80, tftlcd_data.width, tftlcd_data.height, 16, "Flash Disk Formatting..."); // 格式化FLASH
		res = f_mkfs("1:", 1, 4096);																						  // 格式化FLASH,1,盘符;1,不需要引导区,8个扇区为1个簇
		if (res == 0)
		{
			f_setlabel((const TCHAR *)"1:PRECHIN");																		  // 设置Flash磁盘的名字为：PRECHIN
			LCD_ShowString(10, 80, tftlcd_data.width, tftlcd_data.height, 16, "Flash Disk Format Finish"); // 格式化完成
		}
		else
			LCD_ShowString(10, 80, tftlcd_data.width, tftlcd_data.height, 16, "Flash Disk Format Error "); // 格式化失败
		delay_ms(1000);
	}
	LCD_Fill(10, 80, tftlcd_data.width, 80 + 16, WHITE); // 清除显示
	while (fatfs_getfree("0:", &total, &free))			  // 得到SD卡的总容量和剩余容量
	{
		LCD_ShowString(10, 80, tftlcd_data.width, tftlcd_data.height, 16, "SD Card Fatfs Error!");
		delay_ms(200);
		LED2 = !LED2;
	}
	LCD_ShowString(10, 80, tftlcd_data.width, tftlcd_data.height, 16, "FATFS OK!");
	LCD_ShowString(10, 100, tftlcd_data.width, tftlcd_data.height, 16, "SD Total Size:     MB");
	LCD_ShowString(10, 120, tftlcd_data.width, tftlcd_data.height, 16, "SD  Free Size:     MB");
	LCD_ShowNum(10 + 8 * 14, 100, total >> 10, 5, 16); // 显示SD卡总容量 MB
	LCD_ShowNum(10 + 8 * 14, 120, free >> 10, 5, 16);	// 显示SD卡剩余容量 MB

	while (1)
	{
		uart_cmd_process(USART_CHANNEL_1);

		i++;
		if (i % 20 == 0)
		{
			LED2 = !LED2;
		}
		delay_ms(10);
	}
}
