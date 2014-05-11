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
    //smtp_send("smtp.163.com",25,"sanguoshenxian@163.com","ada-sgsx","mysubject","mycontent",to,to_len);
    smtp_send("smtp.qq.com",25,"465444136@qq.com","yanju2013","mysubject","mycontent",to,to_len);

    return EXIT_SUCCESS;
}
