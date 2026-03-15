#include "parse_cmd.h"

char cmd_str_buf[64];

u8 init_cmd_data(struct cmd_data* data){
	data->cmd=NULL;
	data->frequence=1;
	data->time=1;
	return 1;
}

static u32 extract_num_from_str(char *str)
{
    u32 num = 0;
    if(str == NULL){
        return 0;
    }
    while(*str != '\0'){
        if(*str >= '0' && *str <= '9'){
            num = num * 10 + (*str - '0');
        }
        str++;
    }
    return num;
}

struct cmd_data parse_cmd(u8 *data_buf,u32 data_len){
	struct cmd_data new_cmd_data;
    init_cmd_data(&new_cmd_data);

    //合法性校验（空指针、长度无效直接返回初始化后的结构体）
    if(data_buf == NULL || data_len < 5){  // 最小长度（5个字符）
        return new_cmd_data;
    }

    // 拷贝原始字符串（避免strtok修改原缓冲区数据）
    char temp_buf[100] = {0};  // 临时缓冲区，长度可根据实际需求调整
    if(data_len >= sizeof(temp_buf)){
        return new_cmd_data;  // 超出临时缓冲区长度，直接返回
    }
    strncpy(temp_buf, (const char*)data_buf, data_len);
	
    // 以'_'为分隔符，分割字符串
    char *token = NULL;
    u8 seg_index = 0;  // 分割后的片段索引（0：指令，1：频率，2：时间）

    // 第一次分割：获取指令类型（如"send"）
    token = strtok(temp_buf, "_");
    while(token != NULL && seg_index < 3){  // 仅处理前3个片段（指令、频率、时间）
        switch(seg_index){
            case 0:  // 片段0：指令类型（如"send"）
                // 此处可扩展多指令判断（如"send"、"stop"、"query"等）
                if(strcmp(token, "send") == 0){
                    new_cmd_data.cmd = "send";  // 赋值指令类型（只读字符串，不可修改）
                    // 若需要可修改的指令字符串，可使用strcpy+动态内存/固定缓冲区
								} else if(strcmp(token, "receive") == 0){
                    new_cmd_data.cmd = "receive";
                } else if(strcmp(token, "stop") == 0){
                    new_cmd_data.cmd = "stop";
                } else if(strcmp(token, "query") == 0){
                    new_cmd_data.cmd = "query";
                }
                break;

            case 1:  // 片段1：频率（如"80khz"、"100hz"、"1mhz"）
                new_cmd_data.frequence = extract_num_from_str(token);  // 提取数字
                // 判断频率单位，进行换算
								if(strstr(token, "hz") != NULL){  // 赫兹（Hz）
                    new_cmd_data.frequence *= 1;
                } else if(strstr(token, "khz") != NULL){  // 千赫兹（1khz=1000Hz）
                    new_cmd_data.frequence *= 1000;
                } else if(strstr(token, "mhz") != NULL){  // 兆赫兹（1mhz=1000000Hz）
                    new_cmd_data.frequence *= 1000000;
                }
                break;

            case 2:  // 片段2：时间（如"5s"、"10ms"）
                new_cmd_data.time = extract_num_from_str(token);  // 提取数字
                // 判断时间单位，可扩展（当前默认s，新增ms只需添加判断）
                if(strstr(token, "ms") != NULL){  // 毫秒（可选扩展，当前默认秒）
                    new_cmd_data.time *= 1;  // 如需换算为秒，可改为 /= 1000
                }
                // 单位"s"无需换算，直接使用提取的数字
                break;

            default:
                break;
        }

        seg_index++;
        token = strtok(NULL, "_");  // 继续分割下一个片段
    }

    return new_cmd_data;
}
