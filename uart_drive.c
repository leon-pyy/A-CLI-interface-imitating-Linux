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
 * 该文件与cmd_func共同实现命令功能
 * 串口驱动需要根据你使用的平台作出调整
 * 输入“ls”显示命令列表
****************************************************************/

/*串口处理*/

#include "uart_drive.h"
#include "cmd_func.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "stdbool.h"

static char log_buf[128]={0};

extern _cmd_table cmd_table[];
extern int cmdnum;

uint8_t rx_buffer[USART_REC_LEN]={0};
uint8_t rx_data;

static uint16_t rx_index = 0;     // 命令长度
static uint16_t cursor_pos = 0;   // 光标位置

bool cmd_deal_ok = 1;

#define CMD_HISTORY_NUM  10
#define CMD_MAX_LEN      USART_REC_LEN

static char cmd_history[CMD_HISTORY_NUM][CMD_MAX_LEN];
static int history_count = 0;
static int history_index = -1;

typedef enum {
    ESC_IDLE = 0,
    ESC_START,
    ESC_BRACKET
} esc_state_t;

static esc_state_t esc_state = ESC_IDLE;

/* 串口1发送 */
void uart1_send(uint8_t *data,uint16_t len){
    HAL_UART_Transmit(&huart1,data,len,50);
}

/* 判断前缀 */
static bool str_start_with(const char *str, const char *prefix){
    while (*prefix){
        if (*str++ != *prefix++)
            return false;
    }
    return true;
}

/* 保存历史命令 */
static void history_save(const char *cmd){
    if (cmd[0] == '\0') return;

    int idx = history_count % CMD_HISTORY_NUM;
    strncpy(cmd_history[idx], cmd, CMD_MAX_LEN - 1);
    cmd_history[idx][CMD_MAX_LEN - 1] = '\0';

    history_count++;
    history_index = history_count;
}

/* 清除当前行 */
static void clear_line(void){
    while (cursor_pos > 0){
        uint8_t bs_seq[3] = {'\b', ' ', '\b'};
        uart1_send(bs_seq, 3);
        cursor_pos--;
    }
}

/* ← */
static void cursor_left(void){
    if (cursor_pos > 0){
        uart1_send((uint8_t *)"\b", 1);
        cursor_pos--;
    }
}

/* → */
static void cursor_right(void){
    if (cursor_pos < rx_index){
        uart1_send(&rx_buffer[cursor_pos], 1);
        cursor_pos++;
    }
}

/* ↑ */
static void cmd_history_up(void){
    if (history_count == 0) return;
    int oldest = history_count - CMD_HISTORY_NUM;
    if (oldest < 0) oldest = 0;

    if (history_index <= oldest)
        return;

    history_index--;

    while (cursor_pos < rx_index){
        cursor_right();
    }
        
    clear_line();

    strcpy((char *)rx_buffer,
           cmd_history[history_index % CMD_HISTORY_NUM]);

    rx_index = strlen((char *)rx_buffer);
    cursor_pos = rx_index;
    uart1_send(rx_buffer, rx_index);
}

/* ↓ */
static void cmd_history_down(void){
    if (history_count == 0) return;

    /* 已经在空行，不能再往下 */
    if (history_index >= history_count)
        return;
    
    history_index++;

    /* 到达空行 */
    if (history_index == history_count)
    {
        while (cursor_pos < rx_index)
            cursor_right();

        clear_line();
        rx_index = 0;
        cursor_pos = 0;
        rx_buffer[0] = '\0';
        return;
    }

    while (cursor_pos < rx_index){
        cursor_right();
    }

    clear_line();
    strcpy((char *)rx_buffer,
           cmd_history[history_index % CMD_HISTORY_NUM]);

    rx_index = strlen((char *)rx_buffer);
    cursor_pos = rx_index;
    uart1_send(rx_buffer, rx_index);
}

/* 串口接收回调 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;

    if (!cmd_deal_ok) goto rx_exit;

    /* 方向键 */
    if (esc_state != ESC_IDLE){
        if (esc_state == ESC_START){
            esc_state = (rx_data == '[') ? ESC_BRACKET : ESC_IDLE;
            goto rx_exit;
        }
        else if (esc_state == ESC_BRACKET){
            esc_state = ESC_IDLE;
            switch (rx_data){
                case 'A': cmd_history_up();   break;
                case 'B': cmd_history_down(); break;
                case 'C': cursor_right();     break;
                case 'D': cursor_left();      break;
                default: break;
            }
            goto rx_exit;
        }
    }

    if (rx_data == 0x1B){
        esc_state = ESC_START;
        goto rx_exit;
    }

    /* 回车 */
    if (rx_data == CMD_CR || rx_data == CMD_LF){
        rx_buffer[rx_index] = '\0';
        history_save((char *)rx_buffer);

        rx_index = 0;
        cursor_pos = 0;
        esc_state = ESC_IDLE;
        cmd_deal_ok = 0;
        goto rx_exit;
    }

    /* 退格（支持行内删除） */
    if (rx_data == CMD_BS){
        if (cursor_pos > 0){
            memmove(&rx_buffer[cursor_pos - 1],
                    &rx_buffer[cursor_pos],
                    rx_index - cursor_pos);

            rx_index--;
            cursor_pos--;

            uart1_send((uint8_t *)"\b", 1);
            uart1_send(&rx_buffer[cursor_pos],
                       rx_index - cursor_pos);
            uart1_send((uint8_t *)" ", 1);

            for (int i = 0; i <= rx_index - cursor_pos; i++)
                uart1_send((uint8_t *)"\b", 1);
        }
        goto rx_exit;
    }

    /* TAB 补全 */
    if (rx_data == CMD_HT){
        uint8_t match = 0;
        const char *last = NULL;

        rx_buffer[rx_index] = '\0';

        for (int i = 0; i < cmdnum; i++){
            if (str_start_with(cmd_table[i].name,
                               (char *)rx_buffer)){
                match++;
                last = cmd_table[i].name;
            }
        }

        if (match == 1 && last){
            const char *p = last + rx_index;
            while (*p && rx_index < USART_REC_LEN - 1){
                rx_buffer[rx_index++] = *p;
                uart1_send((uint8_t *)p, 1);
                p++;
            }
            cursor_pos = rx_index;
        }
        else if (match > 1){
            const char nl[] = "\r\n";
            uart1_send((uint8_t *)nl, 2);
            for (int i = 0; i < cmdnum; i++){
                if (str_start_with(cmd_table[i].name,
                                   (char *)rx_buffer)){
                    uart1_send((uint8_t *)cmd_table[i].name,
                               strlen(cmd_table[i].name));
                    uart1_send((uint8_t *)nl, 2);
                }
            }
            const char prompt[] = "[leon]@leon:";
            uart1_send((uint8_t *)prompt, strlen(prompt));
            uart1_send(rx_buffer, rx_index);
        }
        goto rx_exit;
    }

    /* 普通可显示字符 */
    if (rx_data >= 0x20 && rx_data <= 0x7E){
        if (rx_index < USART_REC_LEN - 1){
            memmove(&rx_buffer[cursor_pos + 1],
                    &rx_buffer[cursor_pos],
                    rx_index - cursor_pos);

            rx_buffer[cursor_pos++] = rx_data;
            rx_index++;

            uart1_send(&rx_buffer[cursor_pos - 1],
                       rx_index - cursor_pos + 1);

            for (int i = 0; i < rx_index - cursor_pos; i++)
                uart1_send((uint8_t *)"\b", 1);
        }
    }

rx_exit:
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

/* 重定义printf到串口1 */
#if (__ARMCC_VERSION >= 6011111)
__asm(".global __use_no_semihosting\n\t");
__asm(".global __ARM_use_no_argv \n\t");
#else
#pragma import(__use_no_semihosting)

struct __FILE{
    int handle;
};
#endif

int _ttywrch(int ch){
    ch = ch;
    return ch;
}

void _sys_exit(int x){
    x = x;
}

char *_sys_command_string(char *cmd, int len){
    return NULL;
}

FILE __stdout;

int fputc(int ch, FILE *f){
    while ((USART1->SR & 0X40) == 0);
		USART1->DR = (uint8_t)ch;
    return ch;
}

/* 调试帮助打印信息函数 */
void sys_log_info(const char *func,const long line,const char *format, ...){
    va_list args;

    va_start(args,format);

    _sys_log_info("[info]@[%s:%ld] ",func,line);

    vsnprintf(log_buf,sizeof(log_buf),format,args);

    _sys_log_info("%s",log_buf);

    va_end(args);
}

void _sys_log_info(const char *format, ...){
    va_list args;

    va_start(args,format);

    vsnprintf(log_buf,sizeof(log_buf),format,args);

    printf("%s",log_buf);

    va_end(args);    

}
