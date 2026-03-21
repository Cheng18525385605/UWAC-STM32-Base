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

    //�Ϸ���У�飨��ָ�롢������Чֱ�ӷ��س�ʼ����Ľṹ�壩
    if(data_buf == NULL || data_len < 5){  // ��С���ȣ�5���ַ���
        return new_cmd_data;
    }

    // ����ԭʼ�ַ���������strtok�޸�ԭ���������ݣ�
    char temp_buf[100] = {0};  // ��ʱ�����������ȿɸ���ʵ���������
    if(data_len >= sizeof(temp_buf)){
        return new_cmd_data;  // ������ʱ���������ȣ�ֱ�ӷ���
    }
    strncpy(temp_buf, (const char*)data_buf, data_len);
	
    // ��'_'Ϊ�ָ������ָ��ַ���
    char *token = NULL;
    u8 seg_index = 0;  // �ָ���Ƭ��������0��ָ�1��Ƶ�ʣ�2��ʱ�䣩

    // ��һ�ηָ��ȡָ�����ͣ���"send"��
    token = strtok(temp_buf, "_");
    while(token != NULL && seg_index < 3){  // ������ǰ3��Ƭ�Σ�ָ�Ƶ�ʡ�ʱ�䣩
        switch(seg_index){
            case 0:  // Ƭ��0��ָ�����ͣ���"send"��
                // �˴�����չ��ָ���жϣ���"send"��"stop"��"query"�ȣ�
                if(strcmp(token, "send") == 0){
                    new_cmd_data.cmd = "send";  // ��ֵָ�����ͣ�ֻ���ַ����������޸ģ�
                    // ����Ҫ���޸ĵ�ָ���ַ�������ʹ��strcpy+��̬�ڴ�/�̶�������
								} else if(strcmp(token, "receive") == 0){
                    new_cmd_data.cmd = "receive";
                } else if(strcmp(token, "stop") == 0){
                    new_cmd_data.cmd = "stop";
                } else if(strcmp(token, "query") == 0){
                    new_cmd_data.cmd = "query";
                }
                break;

            case 1:  // Ƭ��1��Ƶ�ʣ���"80khz"��"100hz"��"1mhz"��
                new_cmd_data.frequence = extract_num_from_str(token);  // ��ȡ����
                // �ж�Ƶ�ʵ�λ�����л���
								if(strstr(token, "hz") != NULL){  // ���ȣ�Hz��
                    new_cmd_data.frequence *= 1;
                } else if(strstr(token, "khz") != NULL){  // ǧ���ȣ�1khz=1000Hz��
                    new_cmd_data.frequence *= 1000;
                } else if(strstr(token, "mhz") != NULL){  // �׺��ȣ�1mhz=1000000Hz��
                    new_cmd_data.frequence *= 1000000;
                }
                break;

            case 2:  // Ƭ��2��ʱ�䣨��"5s"��"10ms"��
                new_cmd_data.time = extract_num_from_str(token);  // ��ȡ����
                // �ж�ʱ�䵥λ������չ����ǰĬ��s������msֻ�������жϣ�
                if(strstr(token, "ms") != NULL){  // ���루��ѡ��չ����ǰĬ���룩
                    new_cmd_data.time *= 1;  // ���軻��Ϊ�룬�ɸ�Ϊ /= 1000
                }
                // ��λ"s"���軻�㣬ֱ��ʹ����ȡ������
                break;

            default:
                break;
        }

        seg_index++;
        token = strtok(NULL, "_");  // �����ָ���һ��Ƭ��
    }

    return new_cmd_data;
}
