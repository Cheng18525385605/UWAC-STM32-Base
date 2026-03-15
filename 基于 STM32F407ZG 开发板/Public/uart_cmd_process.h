#ifndef _UART_CMD_PROCESS_H
#define _UART_CMD_PROCESS_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parse_cmd.h"
#include "usart.h"
#include "led.h"
#include "pwm.h"
#include "SysTick.h"

extern struct cmd_data my_cmd_data;
typedef enum {
    CMD_UNKNOWN = 0, // 未知指令
	  CMD_SEND_PWM,    // 上位机发送命令
    CMD_RECEIVE_DATA    // 上位机接收数据
    
    
} Cmd_Type;

// ********** 函数声明 **********
void uart_cmd_process(USART_Channel_t channel); // 指令处理主函数
void cmd_action_send_pwm(void); // 发送pwm波形动作
void cmd_action_receive_data(void); // 发送adc采样数据
void uart_send_response(USART_TypeDef*  ,char *resp); // 串口发送应答信息
#endif



