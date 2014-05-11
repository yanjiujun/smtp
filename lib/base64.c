/*
 * lib/base64.c
 *
 *  Created on: 2012-3-1
 *      Author: yanjiujun@gmail.com
 *
 */

#include "base64.h"

#include <stdlib.h>
#include <stdio.h>

#define BASE64_ENCODE_LEN 65
#define BASE64_DECODE_LEN 128

static unsigned char base64_encode_table[BASE64_ENCODE_LEN] = // base64编码表，共65个，最后一个表示空，提供从数据到字符的映射
{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
  'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
  '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '=' };

static unsigned char base64_decode_table[BASE64_DECODE_LEN];	// base64解码表，共128个，提供从字符到数据的映射




// base64编码函数,将输入的数据，每3个（24bit），拆分成4个字节表示，每
// 个字节包含6位有效数据，然后将得到的结果按照编码表转换成字符
// 如果出错返回负数，否则返回编码后的字节数(不包含结尾的null)
int base64_encode(const unsigned char* input,		// 输入数据
                  int input_length,			        // 输入数组的长度
                  unsigned char* output,            // 输出
                  int output_length                 // 输出缓冲区的大小
                 )
{
    int n;
    int index;
    int length2;
    int remainder;
    int encode_length;

    if (!input || input_length <= 0) return -1;

    encode_length = input_length / 3;
    length2 = encode_length * 3;
    remainder = input_length - length2;
    if (remainder) encode_length++;
    encode_length <<= 2;

    if(output_length < encode_length + 1) return -2;

    for (n = 0,index = 0; n < length2; n += 3,index += 4)
    {
        output[index] = base64_encode_table[input[n] >> 2];
        output[index + 1] = base64_encode_table[((input[n] & 0x3) << 4) | (input[n + 1] >> 4)];
        output[index + 2] = base64_encode_table[((input[n + 1] & 0xF) << 2)
                        | ((input[n + 2] & 0xC0) >> 6)];
        output[index + 3] = base64_encode_table[input[n + 2] & 0x3F];
    }

    // 如果不是3的倍数，这里做一点特殊处理
    switch (remainder)
    {
        case 1:
            output[index] = base64_encode_table[input[n] >> 2];
            output[index + 1] = base64_encode_table[(input[n] & 0x3) << 4];
            output[index + 2] = base64_encode_table[BASE64_ENCODE_LEN - 1];
            output[index + 3] = base64_encode_table[BASE64_ENCODE_LEN - 1];
            break;
        case 2:
            output[index] = base64_encode_table[input[n] >> 2];
            output[index + 1] = base64_encode_table[((input[n] & 0x3) << 4) | (input[n + 1] >> 4)];
            output[index + 2] = base64_encode_table[((input[n + 1] & 0xF) << 2)];
            output[index + 3] = base64_encode_table[BASE64_ENCODE_LEN - 1];
            break;
        default:
            break;
    }

    output[encode_length] = 0;

    return encode_length;
}

// 解码函数,如果输入不是4的倍数，则认为输入字符串不合法
// 如果出错，返回一个负数，否则返回解码后的数据长度(不包含结尾的null)
int base64_decode(const unsigned char* input,	// 输入数据
                  int input_length,             // 输入数据的长度
                  unsigned char* output,        // 解码输出的缓冲区
                  int output_length             // 缓冲区大小
                  )
{
    int n;
    int index;
    int length2;
    int remainder;
    int decode_length;

    // 如果长度不是4的倍数，则直接返回，因为base64编码结果一定是
    // 4的倍数
    if (!input || input_length <= 0 || (input_length & 0x3)) return -1;

    // 如果解码表数据不正确（一般是第一次执行本函数），重新初始化
    if (base64_decode_table['='] != BASE64_ENCODE_LEN - 1)
    {
        for (n = 0; n < BASE64_ENCODE_LEN; n++) base64_decode_table[base64_encode_table[n]] = n;
    }

    // 如果最后有补空符号，则数据长度减少,长度+1包含了结尾的null
    decode_length = (input_length >> 2) * 3;
    if (input[input_length - 1] == base64_encode_table[BASE64_ENCODE_LEN - 1]) decode_length--;
    if (input[input_length - 2] == base64_encode_table[BASE64_ENCODE_LEN - 1]) decode_length--;

    if(output_length < decode_length + 1) return -2;

    length2 = decode_length / 3 * 3;
    remainder = decode_length - length2;

    for (n = 0,index = 0; n < length2; n += 3,index += 4)
    {
        output[n] = (base64_decode_table[input[index]] << 2) | (base64_decode_table[input[index + 1]] >> 4);
        output[n + 1] = (base64_decode_table[input[index + 1]] << 4) | (base64_decode_table[input[index + 2]] >> 2);
        output[n + 2] = (base64_decode_table[input[index + 2]] << 6) | base64_decode_table[input[index + 3]];
    }

    // 处理余数部分的字节
    switch (remainder)
    {
        case 1:
            output[n] = (base64_decode_table[input[index]] << 2) | (base64_decode_table[input[index + 1]] >> 4);
            break;
        case 2:
            output[n] = (base64_decode_table[input[index]] << 2) | (base64_decode_table[input[index + 1]] >> 4);
            output[n + 1] = (base64_decode_table[input[index + 1]] << 4) | (base64_decode_table[input[index + 2]] >> 2);
            break;
    }

    output[decode_length] = 0;

    return decode_length;
}

