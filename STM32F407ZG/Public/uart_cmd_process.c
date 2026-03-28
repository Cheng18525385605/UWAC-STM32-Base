#include "uart_cmd_process.h"
#include "time.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "wav.h"
#include "ff.h"
#define USART_RX_BUF_LEN 128 // 接收缓冲区大小，可按需调整
#define TIM_CLOCK_FREQ 84000000

struct cmd_data my_cmd_data;

// 发送wav文件调制的pwm波形动作
void cmd_action_send_wav(void)
{
    DIR wavdir;
    FILINFO *wavfileinfo; // 文件信息
    u16 totwavnum;        // 音乐文件总数
    u8 *pname;            // 带路径的文件名
    u32 *wavoffsettbl;    // 音乐offset索引表
    u8 res;
    u16 curindex; // 当前索引
    u32 temp;

    // 读取wav文件到buf中，从buf中通过dma往pwm的寄存器中传输数据
    // 第一步打开声音文件，首先判断是否有wav文件再music目录下。
    while (f_opendir(&wavdir, "0:/MUSIC")) // 打开音乐文件夹
    {
        uart_send_response(USART1, "open 0:/MUSIC error"); // 向上位机返回应答
        return;
    }
    totwavnum = audio_get_tnum("0:/MUSIC"); // 得到总有效文件数
    printf("number of music file:%d", totwavnum);
    wavfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO)); // 申请内存
    wavoffsettbl = mymalloc(SRAMIN, 4 * totwavnum);             // 申请4*totwavnum个字节的内存,用于存放音乐文件off block索引
    pname = mymalloc(SRAMIN, _MAX_LFN * 2 + 1);                 // 为带路径的文件名分配内存
    while (!wavfileinfo || !pname || !wavoffsettbl)             // 内存分配出错
    {
        printf("mymalloc fail 内存分配失败");
        return;
    }
    res = f_opendir(&wavdir, "0:/MUSIC"); // 打开目录
    if (res == FR_OK)
    {
        curindex = 0; // 当前索引为0
        while (1)     // 全部查询一遍
        {
            temp = wavdir.dptr;                    // 记录当前index
            res = f_readdir(&wavdir, wavfileinfo); // 读取目录下的一个文件
            if (res != FR_OK || wavfileinfo->fname[0] == 0)
                break; // 错误了/到末尾了,退出
            res = f_typetell((u8 *)wavfileinfo->fname);
            if ((res & 0XF0) == 0X40) // 取高四位,看看是不是音乐文件
            {
                wavoffsettbl[curindex] = temp; // 记录索引
                curindex++;
            }
        }
    }
    curindex = 0;                                        // 从0开始显示
    res = f_opendir(&wavdir, (const TCHAR *)"0:/MUSIC"); // 打开目录

    dir_sdi(&wavdir, wavoffsettbl[curindex]); // 改变当前目录索引
    res = f_readdir(&wavdir, wavfileinfo);    // 读取目录下的一个文件
    if (res != FR_OK || wavfileinfo->fname[0] == 0)
        return;                                              // 错误了/到末尾了,退出
    strcpy((char *)pname, "0:/MUSIC/");                      // 复制路径(目录)
    strcat((char *)pname, (const char *)wavfileinfo->fname); // 将文件名接在后面
    if (pname != NULL)
    {
        printf("file:%s\r\n", pname);
    }
    else
    {
        printf("文件名解析失败！\r\n");
    }
    res = start_wav_pwm((char *)pname); // 开始wav转换为
    // if(key==KEY2_PRESS)		//上一曲
    // {
    // 	if(curindex)curindex--;
    // 	else curindex=totwavnum-1;
    // }else if(key==KEY0_PRESS)//下一曲
    // {
    // 	curindex++;
    // 	if(curindex>=totwavnum)curindex=0;//到末尾的时候,自动从头开始
    // }else break;	//产生了错误

    myfree(SRAMIN, wavfileinfo);  // 释放内存
    myfree(SRAMIN, pname);        // 释放内存
    myfree(SRAMIN, wavoffsettbl); // 释放内存
    return;
}

// 发送pwm波形动作
void cmd_action_send_pwm(void)
{
    ADC_Cmd(ADC1, ENABLE);
    ADC_DMA_Trig(ADC1_DMA_Size); // 开始AD采集，参数为采样点数
    TIM_Cmd(TIM3, ENABLE);       // 开启adc的触发时钟   adc+dma

    delay_ms(500);                             // 等待
    uart_send_response(USART1, "send_pwm_OK"); // 向上位机返回应答
    tim4_count = my_cmd_data.time;             // 设置定时计数变量，用来设置pwm的发送时间，单位是由TIM4的中断间隔设定
    TIM4_Init(10000, 8399);                    // 设置定时1s间隔  Tout= ((per)*(psc+1))/Tclk；

    u32 arr_val;
    u16 psc_val;
    u32 target_freq = my_cmd_data.frequence;
    if (target_freq < 100) // 如果频率很低 (<100Hz)
    {
        psc_val = 8399; // 分频到 10kHz
        arr_val = (10000 / target_freq) - 1;
    }
    else if (target_freq < 20000) // 中等频率 (<20kHz)
    {
        psc_val = 83; // 分频到 1MHz (84M / 84)
        arr_val = (1000000 / target_freq) - 1;
    }
    else // 高频 (>= 20kHz)，比如你的 80kHz
    {
        psc_val = 0; // 不分频，时钟跑在 84MHz
        arr_val = (TIM_CLOCK_FREQ / target_freq) - 1;
    }

    // 防止 ARR 超过 16位定时器的最大值 65535
    if (arr_val > 65535)
        arr_val = 65535;

    TIM14_CH1_PWM_Init(arr_val, psc_val); // 开始发送pwm
    //  开始采集ADC数据并保存为WAV
    adc_save_to_wav("ADC_PWM_REC1.WAV");
    delay_ms(my_cmd_data.time * 1000 + 500); // 等待
    TIM_Cmd(TIM3, DISABLE);                  // 关闭adc的触发时钟   adc+dma//关闭adc+dma
}

// 发送adc采样数据
void cmd_action_receive_data(void)
{
    LED1 = 1;
    uart_send_response(USART2, "receive_data_OK");
}

// 串口发送应答信息（复用你的串口发送逻辑）
void uart_send_response(USART_TypeDef *USARTx, char *resp)
{
    if (resp == NULL)
        return;
    u16 send_len = strlen(resp);
    // 发送应答字符串
    for (u16 t = 0; t < send_len; t++)
    {
        USART_SendData(USARTx, resp[t]);
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET)
            ;
    }
    // 发送换行符，便于上位机识别
    USART_SendData(USARTx, '\r');
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET)
        ;
    USART_SendData(USARTx, '\n');
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) != SET)
        ;
}

void uart_cmd_process(USART_Channel_t channel)
{
    u16 len = 0;
    Cmd_Type cmd_type = CMD_UNKNOWN;

    // 根据选择的串口确定使用的变量
    u16 *p_rx_sta;
    u8 *p_rx_buf;
    USART_TypeDef *p_usart;

    switch (channel)
    {
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

    if (*p_rx_sta & 0x8000)
    {                             // 接收完成
        len = *p_rx_sta & 0x3fff; // 得到此次接收到的数据长度

        // 解析命令
        my_cmd_data = parse_cmd(p_rx_buf, len);

        // 判断指令类型
        if (my_cmd_data.cmd != NULL)
        {
            if (strcmp(my_cmd_data.cmd, "send") == 0)
            {
                if (my_cmd_data.frequence != 0)
                    cmd_type = CMD_SEND_PWM;
                else
                    cmd_type = CMD_SEND_WAV;
            }
            else if (strcmp(my_cmd_data.cmd, "receive") == 0)
            {
                cmd_type = CMD_RECEIVE_DATA;
            }
            else
            {
                cmd_type = CMD_UNKNOWN;
            }
        }

        // 根据指令类型执行对应动作
        switch (cmd_type)
        {
        case CMD_SEND_PWM:
            cmd_action_send_pwm();
            break;
        case CMD_RECEIVE_DATA:
            cmd_action_receive_data();
            break;
        case CMD_UNKNOWN:
            uart_send_response(p_usart, "UNKNOWN_CMD");
            break;
        case CMD_SEND_WAV:
            cmd_action_send_wav();
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
