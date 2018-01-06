/**
 * @file smtp.c
 *
 *  Created on: 2014年4月28日
 *      Author: yanjiujun@gmail.com
 *
 * @brief 实现smtp发送功能。这个模块是多线程安全的。
 *        即使是时间函数也是使用的localtime_r。
 *
 *        修复了bug：
 *              DATA HELLO 这样的命令之后不需要加空格，如果有空格，有些服务器会报错。
 *              服务器登录响应的返回值，可能是Username: 或者username:,不需要区分大
 *              小写。
 *                                                          2014/5/11
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "smtp.h"
#include "lib/base64.h"

/**
 * 返回的错误码
 */
enum SMTP_ERROR
{
    /**
     * @brief 成功
     */
    SMTP_ERROR_OK,

    /**
     * @brief 创建套接字失败
     */
    SMTP_ERROR_SOCKET,

    /**
     * @brief 链接服务器失败
     */
    SMTP_ERROR_CONNECT,

    /**
     * @brief 域名解析错误
     */
    SMTP_ERROR_DOMAIN,

    /**
     * @brief 读取服务器信息错误
     */
    SMTP_ERROR_READ,

    /**
     * @brief 发送数据错误
     */
    SMTP_ERROR_WRITE,

    /**
     * @brief 服务器状态错误
     */
    SMTP_ERROR_SERVER_STATUS
};

#define SMTP_BUFFER_SIZE 1024

/**
 * @struct smtp
 */
struct smtp
{
    /**
     * @brief 域名
     */
    const char* domain;

    /**
     * @brief 用户名
     */
    const char* user_name;

    /**
     * @brief 密码
     */
    const char* password;

    /**
     * @brief 标题
     */
    const char* subject;

    /**
     * @brief 内容
     */
    const char* content;

    /**
     * @brief 发送目标，可以是多个目标
     */
    const char** to;

    /**
     * @brief 目标数量
     */
    int to_len;

    /**
     * @brief smtp协议当前状态
     */
    int status;

    /**
     * @brief 使用的套结字
     */
    int socket;

    /**
     * @brief 用于接收的缓冲区，接收完毕后会自动解析出响应状态和数据分别存储在cmd和data节点中
     */
    char buffer[SMTP_BUFFER_SIZE];

    /**
     * @brief 接收到的响应状态，这是一个指向缓冲区的指针
     */
    char* cmd;

    /**
     * @brief 接收到的数据，这是一个指向缓冲区的指针
     */
    char* data;
};

/**
 * @enum smtp 状态
 */
enum SMTP_STATUS
{
    SMTP_STATUS_NULL,     //!< SMTP_STATUS_NULL
    SMTP_STATUS_EHLO,     //!< SMTP_STATUS_EHLO
    SMTP_STATUS_AUTH,     //!< SMTP_STATUS_AUTH
    SMTP_STATUS_SEND,     //!< SMTP_STATUS_SEND
    SMTP_STATUS_QUIT,     //!< SMTP_STATUS_QUIT
    SMTP_STATUS_MAX       //!< SMTP_STATUS_MAX
};

/**
 * @brief 读取smtp服务器响应并简单解析出响应状态和响应参数
 * @param sm    smtp指针
 * @return 读取正常返回0，否则返回正数表示错误原因
 */
static int smtp_read(struct smtp* sm)
{
    for(;;)
    {
        int size = recv(sm->socket,sm->buffer,SMTP_BUFFER_SIZE - 1,0);
        if(size == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
        }

        // 套结字错误或者关闭
        if(size <= 0) break;

        sm->buffer[size] = 0;
        printf("SERVER: %s\n",sm->buffer);

        // 不确定这个是否找不到，smtp协议一般都会在响应状态后跟随参数
        sm->cmd = sm->buffer;
        char* p = strchr(sm->buffer,' ');
        if(p)
        {
            *p = '\0';
            sm->data = p + 1;
        }

        return 0;
    }

    printf("smtp_read() 接收信息错误\n");

    return SMTP_ERROR_READ;
}

/**
 * @brief 向服务器发送信息
 * @param fd            套结字
 * @param buffer        要发送的数据
 * @param buffer_size   要发送的数据长度
 * @return  如果成功反送返回0，否则返回正数表示错误原因
 */
int smtp_write(int fd,const char* buffer,int size)
{
    for(int send_num = 0;send_num < size; )
    {
        int error = send(fd,&buffer[send_num],size - send_num,0);
        if(error < 0)
        {
            printf("发送数据错误 errno = %d size = %d send_num = %d",errno,size, send_num);
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            return SMTP_ERROR_WRITE;
        }
        else send_num += error;
    }

    printf("CLIENT: %s\n",buffer);

    return 0;
}

/**
 * @brief 分割接收到的数据，主要是区分base64编码结果。同时sm->data节点会被修改
 * @param sm
 * @return 返回原来的sm->data。
 */
static char* explode(struct smtp* sm)
{
    char* old = sm->data;
    char* p = old;
    while(*p)
    {
        if((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
               *p == '+' || *p == '/' || *p == '=')
        {
            p++;
        }
        else
        {
            sm->data = p;
            *p = '\0';
            break;
        }
    }

    return old;
}

static int hello(struct smtp* sm)
{
    // 发送HELO命令
    char buffer[256];
    int size = sprintf(buffer,"HELO %s\r\n",sm->domain);

    if(smtp_write(sm->socket,buffer,size)) return SMTP_ERROR_WRITE;

    // 服务器应该正常返回250
    if(smtp_read(sm) || strcmp(sm->cmd,"250")) return SMTP_ERROR_READ;

    sm->status = SMTP_STATUS_AUTH;

    return 0;
}

static int auth(struct smtp* sm)
{
    // 发送AUTH命令,第一次接到的数据应该是base64编码后的Username，如果不是直接返回
    // 然后第二次应该是base64后的Password
    if(smtp_write(sm->socket,"AUTH LOGIN\r\n",strlen("AUTH LOGIN\r\n"))) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"334")) return SMTP_ERROR_READ;

    // 用户名
    char* p = explode(sm);
    char buffer[256];
    int size = base64_decode(p,strlen(p),buffer,256);
    if(size < 0) return SMTP_ERROR_SERVER_STATUS;

    buffer[size] = 0;

    // 这里并非锁有时候都是"Username:",我测试的163居然在半个月后由Username变成username了
    if(strcasecmp(buffer,"username:")) return SMTP_ERROR_SERVER_STATUS;
    size = base64_encode(sm->user_name,strlen(sm->user_name),buffer,256);
    if(size < 0 || size + 2 > 256) return SMTP_ERROR_WRITE;
    buffer[size++] = '\r';
    buffer[size++] = '\n';
    buffer[size] = '\0';
    if(smtp_write(sm->socket,buffer,size)) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"334")) return SMTP_ERROR_READ;

    // 密码
    p = explode(sm);
    size = base64_decode(p,strlen(p),buffer,256);
    if(size < 0) return SMTP_ERROR_SERVER_STATUS;
    buffer[size] = 0;
    if(strcasecmp(buffer,"password:")) return SMTP_ERROR_SERVER_STATUS;
    size = base64_encode(sm->password,strlen(sm->password),buffer,256);
    if(size < 0 || size + 2 > 256) return SMTP_ERROR_WRITE;
    buffer[size++] = '\r';
    buffer[size++] = '\n';
    buffer[size] = '\0';
    if(smtp_write(sm->socket,buffer,size)) return SMTP_ERROR_WRITE;

    if(smtp_read(sm) || strcmp(sm->cmd,"235")) return SMTP_ERROR_READ;
    sm->status = SMTP_STATUS_SEND;

    return 0;
}

/**
 * @brief 生产smtp格式的时间字符串:Tue, 29 Apr 2014 15:02:37 +0800
 *        这里直接返回在堆栈上的缓冲区，意味着只能立刻使用，一旦堆栈有变
 *        动就不能在使用了。
 * @param buffer
 * @return 返回 buffer
 */
static char* smtp_time(char* buffer)
{
    time_t t2;
    time(&t2);

    struct tm t;
    localtime_r(&t2,&t);

    // RFC 5322 date-time section 3.3.
    strftime(buffer, 32, "%a, %d %b %Y %H:%M:%S %z", &t);

    return buffer;
}

static int send_mail(struct smtp* sm)
{
    // MAIL FROM
    char buffer[256];
    int size = sprintf(buffer,"MAIL FROM: <%s>\r\n",sm->user_name);
    if(smtp_write(sm->socket,buffer,size)) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"250")) return SMTP_ERROR_READ;

    // RCPT TO
    int i;
    for(i = 0; i < sm->to_len; i++)
    {
        size = sprintf(buffer,"RCPT TO: <%s>\r\n",sm->to[i]);
        if(smtp_write(sm->socket,buffer,size)) return SMTP_ERROR_WRITE;
        if(smtp_read(sm) || strcmp(sm->cmd,"250")) return SMTP_ERROR_READ;
    }

    // DATA,最后一行是 "\r\n.\r\n" 表示邮件结束
    if(smtp_write(sm->socket,"DATA\r\n",strlen("DATA\r\n"))) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"354")) return SMTP_ERROR_READ;

    // 分配足够大缓冲区存储邮件头，唯一不确定的就是群发数量，这里先统计一下发送目标占用的字节大小
    int to_size = 0;
    for(i = 0; i < sm->to_len; i++) to_size += strlen(sm->to[i]);

    char header[to_size + 512 + strlen(sm->user_name)];
    int pos = strlen("MIME-Version: 1.0\r\nContent-Type: text/html\r\n");
    memcpy(header,"MIME-Version: 1.0\r\nContent-Type: text/html\r\n",pos);

    for(i = 0; i < sm->to_len; i++)
    {
        pos += sprintf(&header[pos],"To: %s\r\n",sm->to[i]);
    }
    pos += sprintf(&header[pos],"From: \"%s\" <%s>\r\n",sm->user_name,sm->user_name);
    pos += sprintf(&header[pos],"Subject: %s\r\n",sm->subject);
    pos += sprintf(&header[pos],"Message-ID: <%d.%s>\r\n",time(NULL),sm->user_name);

    char date[100];
    pos += sprintf(&header[pos],"Date: %s\r\n",smtp_time(date));

    // Must have blank line after header, before message body.
    pos += sprintf(&header[pos],"\r\n");

    if(smtp_write(sm->socket,header,pos)) return SMTP_ERROR_WRITE;
    if(smtp_write(sm->socket,sm->content,strlen(sm->content))) return SMTP_ERROR_WRITE;
    if(smtp_write(sm->socket,"\r\n.\r\n",strlen("\r\n.\r\n"))) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"250")) return SMTP_ERROR_READ;

    sm->status = SMTP_STATUS_QUIT;

    return 0;
}

static int quit(struct smtp* sm)
{
    if(smtp_write(sm->socket,"QUIT\r\n",strlen("QUIT\r\n"))) return SMTP_ERROR_WRITE;
    if(smtp_read(sm) || strcmp(sm->cmd,"221")) return SMTP_ERROR_READ;

    sm->status = SMTP_STATUS_NULL;

    return 0;
}

typedef int (*SMTP_FUN)(struct smtp*);
static const SMTP_FUN smtp_fun[SMTP_STATUS_MAX] = {NULL,hello,auth,send_mail,quit};

int smtp_send(const char* domain,int port,const char* user_name,const char* password,
              const char* subject,const char* content,const char** to,int to_len)
{
    struct hostent* host = gethostbyname(domain);
    if(!host)
    {
        printf("domain can not find！\n");
        return SMTP_ERROR_DOMAIN;
    }

    if(host->h_addrtype != AF_INET)
    {
        if(host->h_addrtype == AF_INET6) printf("ipv6 is not support!\n");
        else printf("address type is not support %d\n ",host->h_addrtype);
        return SMTP_ERROR_DOMAIN;
    }

    int sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(-1 == sock_fd)
    {
        printf("can not create socket!\n");
        return SMTP_ERROR_SOCKET;
    }

    // 连接到服务器
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr = *(struct in_addr*)host->h_addr_list[0];
    memset(local.sin_zero,0,sizeof(local.sin_zero));

    if(-1 == connect(sock_fd,(struct sockaddr*)&local,sizeof(local)))
    {
        printf("can not connect socket!\n");
        return SMTP_ERROR_CONNECT;
    }

    printf("connect ok ,ip address %s \n",inet_ntoa(local.sin_addr));

    struct smtp sm = {.domain=domain,.user_name=user_name,.password=password,.subject=subject,
                      .content=content,.status=SMTP_STATUS_EHLO,.socket=sock_fd,.to=to,
                      .to_len=to_len};

    // 这里应该是服务器欢迎信息，如果状态不是220，直接返回
    if(smtp_read(&sm) || strcmp(sm.cmd,"220")) return SMTP_ERROR_READ;

    while(sm.status != SMTP_STATUS_NULL)
    {
        int error = smtp_fun[sm.status](&sm);
        if(error)
        {
            printf("error = %d\n",error);
            return error;
        }
    }

    close(sock_fd);

    return 0;
}
