#ifndef _PARSE_CMD_H
#define _PARSE_CMD_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// 定义拼接缓冲区（静态数组，避免局部变量栈溢出，初始化清零）
extern char cmd_str_buf[64] ; 

struct cmd_data{
	char* cmd;
	int frequence;
	int time;
	char parameter[10];
};

struct cmd_data parse_cmd(u8 *,u32 );

#endif
