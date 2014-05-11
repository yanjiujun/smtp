/**
 * @file lib/base64.h
 *
 *  Created on: 2012-3-1
 *      Author: yanjiujun@gmail.com
 *
 * @brief base64编码解码
 *
 */

#ifndef BASE64_H_
#define BASE64_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief base64编码
 * @param input         要编码的数据
 * @param input_length  要编码的数据长度
 * @param output        编码后的输出缓冲区
 * @param output_length 输出缓冲区的长度
 * @return
 */
int base64_encode(const unsigned char* input,int input_length,unsigned char* output,int output_length);

/**
 * @brief base64解码
 * @param input         要解码的数据
 * @param input_length  要解码的数据长度
 * @param output        解码后的输出缓冲区
 * @param output_length 输出缓冲区的长度
 * @return
 */
int base64_decode(const unsigned char* input,int input_length,unsigned char* output,int output_length);

#ifdef __cplusplus
}
#endif

#endif /* BASE64_H_ */
