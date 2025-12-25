// MIT License

// Copyright (c) [2023] [leon-pyy]

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/****************************************************************
 * 该文件与uart_drive共同实现命令功能
 * 串口驱动需要根据你使用的平台作出调整
****************************************************************/
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include "uart_drive.h"
#include "usart.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"
#include "cmd_func.h"

extern uint8_t rx_buffer[USART_REC_LEN];//串口一次接受的字符
char token[CMD_PARMNUM][CMD_LONGTH]={0};//命令缓存
extern bool cmd_deal_ok;//锁，上锁为1

//命令执行的具体函数示例
void caclu_add(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("add = %d \r\n",parm1+parm2);
}

void caclu_sub(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("sub = %d \r\n",parm1-parm2);
}

void caclu_mul(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("mul = %d \r\n",parm1*parm2);
}

void caclu_div(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("div = %d \r\n",parm1/parm2);
}

//命令列表，新的命令添加到这里
//命令对应的函数-命令名-命令说明
_cmd_table cmd_table[]={
    (void *)caclu_add,"add","add [parm1] [parm2]",
    (void *)caclu_sub,"sub","sub [parm1] [parm2]",
    (void *)caclu_mul,"mul","mul [parm1] [parm2]",
    (void *)caclu_div,"div","div [parm1] [parm2]",
};

//计算命令的数量
int cmdnum = sizeof(cmd_table)/sizeof(_cmd_table); 

//执行命令
void execute_cmd(void){
    int cmd_is_find = 0;//标记是否找到命令
    if(strlen(token[0])!=0){
        if(!strcmp(token[0],"ls")){//如果输入的是打印命令列表
			printf("-------------------- cmd table --------------------\r\n");
            for(int i=0;i<cmdnum;i++){
                printf("cmd:%s    eg:%s\r\n",cmd_table[i].name,cmd_table[i].example);
            }
            printf("---------------------------------------------------\r\n");
        }
        else{
            for(int j=0;j<cmdnum;j++){//找寻需要执行的命令
                if(!strcmp(token[0],cmd_table[j].name)){
                    cmd_table[j].func();//执行命令函数
                    cmd_is_find++;
                }
            }
            if(cmd_is_find==0){//未找到命令
                printf("cmd error!\r\n");
            }
        }
    }

    //命令执行完毕，清空命令缓存
	memset(token,0,CMD_PARMNUM*CMD_LONGTH);
}

//命令处理函数，放在mian的while(1)中
void process_cmd(void){
    char tmp_data='\0';
    int buf_count=0;
    int parm_count=0;
    int str_count=0;

    if(cmd_deal_ok==0){
		int rx_len = strlen((const char*)rx_buffer);
        for(int i=0;i<rx_len;i++){
            tmp_data = rx_buffer[buf_count];
            if((tmp_data!='\r')||(tmp_data!='\n')||(tmp_data!='\0')){
                if(tmp_data!=' '){
                    token[parm_count][str_count] = tmp_data;
                    if(str_count<CMD_LONGTH-1){
                        str_count++;
                    }
                }
                else{
                    token[parm_count][str_count] = '\0';//将空格替换为字符串结束符
                    if(parm_count<CMD_PARMNUM-1){
                        parm_count++;
                    }
                    str_count = 0;
                }
            }
            if(buf_count<USART_REC_LEN-1){
                buf_count++;
            }
        }
		printf("\r\n");
        execute_cmd();//执行命令
        memset(rx_buffer,0,USART_REC_LEN);//清空串口buffer
        cmd_deal_ok = 1;//解锁
        printf("[leon]@leon:");
    }

}
