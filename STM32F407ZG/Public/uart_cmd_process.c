#include "uart_cmd_process.h"
#include "time.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"	
#include "wav.h"
#define USART_RX_BUF_LEN 128 // 接收缓冲区大小，可按需调整
#define TIM_CLOCK_FREQ  84000000

struct cmd_data my_cmd_data;
// 发送pwm波形动作
void cmd_action_send_pwm(void)
{
    ADC_Cmd(ADC1, ENABLE);
    ADC_DMA_Trig( ADC1_DMA_Size );//开始AD采集，参数为采样点数
    TIM_Cmd( TIM3 , ENABLE );//开启adc的触发时钟   adc+dma
    
    delay_ms(500);	//等待
    uart_send_response(USART1,"send_pwm_OK"); // 向上位机返回应答
    tim4_count=my_cmd_data.time; //设置定时计数变量，用来设置pwm的发送时间，单位是由TIM4的中断间隔设定
    TIM4_Init(10000,8399);     //设置定时1s间隔

    u32 arr_val;
    u16 psc_val;
    u32 target_freq = my_cmd_data.frequence;
		if(target_freq < 100) // 如果频率很低 (<100Hz)
    {
        psc_val = 8399;   // 分频到 10kHz
        arr_val = (10000 / target_freq) - 1;
    }
    else if(target_freq < 20000) // 中等频率 (<20kHz)
    {
        psc_val = 83;     // 分频到 1MHz (84M / 84)
        arr_val = (1000000 / target_freq) - 1;
    }
    else // 高频 (>= 20kHz)，比如你的 80kHz
    {
        psc_val = 0;      // 不分频，时钟跑在 84MHz
        arr_val = (TIM_CLOCK_FREQ / target_freq) - 1;
    }

    // 防止 ARR 超过 16位定时器的最大值 65535
    if(arr_val > 65535) arr_val = 65535;
	
		TIM14_CH1_PWM_Init(arr_val, psc_val); //开始发送pwm
		//  开始采集ADC数据并保存为WAV
		adc_save_to_wav("ADC_PWM_REC1.WAV");
		delay_ms(my_cmd_data.time*1000+500);   //等待
		TIM_Cmd( TIM3 , DISABLE );//关闭adc的触发时钟   adc+dma//关闭adc+dma

}

// 发送adc采样数据
void cmd_action_receive_data(void)
{
    LED1 = 1;
    uart_send_response(USART2,"receive_data_OK");
}

// 串口发送应答信息（复用你的串口发送逻辑）
void uart_send_response(USART_TypeDef* USARTx ,char *resp)
{
    if(resp == NULL) return;
    u16 send_len = strlen(resp);
    // 发送应答字符串
    for(u16 t=0; t<send_len; t++)
    {
        USART_SendData(USARTx, resp[t]);
        while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET);
    }
    // 发送换行符，便于上位机识别
    USART_SendData(USARTx, '\r');
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET);
    USART_SendData(USARTx, '\n');
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET);
}


void uart_cmd_process(USART_Channel_t channel)
{
    u16 len = 0;
    Cmd_Type cmd_type = CMD_UNKNOWN;
    
    // 根据选择的串口确定使用的变量
    u16* p_rx_sta;
    u8* p_rx_buf;
    USART_TypeDef* p_usart;
    
    switch(channel) {
        case USART_CHANNEL_1:
            p_rx_sta = &USART1_RX_STA;
            p_rx_buf = USART1_RX_BUF;
            p_usart = USART1;
            break;
        case USART_CHANNEL_2:
            p_rx_sta = &USART2_RX_STA;
            p_rx_buf = USART2_RX_BUF;
            p_usart = USART2;
            break;
        default:
            return; // 无效的串口号
    }
    
    if(*p_rx_sta & 0x8000) { // 接收完成
        len = *p_rx_sta & 0x3fff; // 得到此次接收到的数据长度
        
        // 解析命令
        my_cmd_data = parse_cmd(p_rx_buf, len);
        
        // 判断指令类型
        if(my_cmd_data.cmd != NULL) {
            if(strcmp(my_cmd_data.cmd, "send") == 0) {
                cmd_type = CMD_SEND_PWM;
            } else if(strcmp(my_cmd_data.cmd, "receive") == 0) {
                cmd_type = CMD_RECEIVE_DATA;
            } else {
                cmd_type = CMD_UNKNOWN;
            }
        }
        
        // 根据指令类型执行对应动作
        switch(cmd_type) {
            case CMD_SEND_PWM:
                cmd_action_send_pwm();
                break;
            case CMD_RECEIVE_DATA:
                cmd_action_receive_data();
                break;
            case CMD_UNKNOWN:
                uart_send_response(p_usart, "UNKNOWN_CMD");
                break;
            default:
                break;
        }
        
        // 清零接收状态和缓冲区，准备下一次接收
        *p_rx_sta = 0;
        
        // 清空缓冲区（避免残留上一次的数据）
        memset(p_rx_buf, 0, len);
    }
}






