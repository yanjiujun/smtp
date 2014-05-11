/**
 * @file main.c
 *
 *  Created on: 2014年4月28日
 *      Author: yanjiujun@gmail.com
 *
 * @brief 测试smtp协议
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "smtp.h"

int main(int argc,char** argv)
{
    const char* to[] = {"yanjiujun@gmail.com","254659936@qq.com"};
    int to_len = 1;

    // 这里需要替换程真实的邮箱账户
    smtp_send("smtp.163.com",25,"username@163.com","password","mysubject","mycontent",to,to_len);

    return EXIT_SUCCESS;
}
