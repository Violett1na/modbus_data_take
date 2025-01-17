/***********************************************************************************
Copy right:	    hqyj Tech.
Author:         jiaoyue
Date:           2023.07.01
Description:    http请求处理
***********************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include "custom_handle.h"

#define KB 1024
#define HTML_SIZE (64 * KB)

// 普通的文本回复需要增加html头部
#define HTML_HEAD "Content-Type: text/html\r\n" \
                  "Connection: close\r\n"

static int handle_login(int sock, const char *input)
{
    char reply_buf[HTML_SIZE] = {0};
    char *uname = strstr(input, "username=");
    uname += strlen("username=");
    char *p = strstr(input, "password");
    *(p - 1) = '\0';
    printf("username = %s\n", uname);

    char *passwd = p + strlen("password=");
    printf("passwd = %s\n", passwd);

    if (strcmp(uname, "admin") == 0 && strcmp(passwd, "admin") == 0)
    {
        sprintf(reply_buf, "<script>localStorage.setItem('usr_user_name', '%s');</script>", uname);
        strcat(reply_buf, "<script>window.location.href = '/index.html';</script>");
        send(sock, reply_buf, strlen(reply_buf), 0);
    }
    else
    {
        printf("web login failed\n");

        //"用户名或密码错误"提示，chrome浏览器直接输送utf-8字符流乱码，没有找到太好解决方案，先过渡
        char out[128] = {0xd3, 0xc3, 0xbb, 0xa7, 0xc3, 0xfb, 0xbb, 0xf2, 0xc3, 0xdc, 0xc2, 0xeb, 0xb4, 0xed, 0xce, 0xf3};
        sprintf(reply_buf, "<script charset='gb2312'>alert('%s');</script>", out);
        strcat(reply_buf, "<script>window.location.href = '/login.html';</script>");
        send(sock, reply_buf, strlen(reply_buf), 0);
    }

    return 0;
}

static int handle_add(int sock, const char *input)
{
    int number1, number2;

    // input必须是"data1=1data2=6"类似的格式，注意前端过来的字符串会有双引号
    sscanf(input, "\"data1=%ddata2=%d\"", &number1, &number2);
    printf("num1 = %d\n", number1);

    char reply_buf[HTML_SIZE] = {0};
    printf("num = %d\n", number1 + number2);
    sprintf(reply_buf, "%d", number1 + number2);
    printf("resp = %s\n", reply_buf);
    send(sock, reply_buf, strlen(reply_buf), 0);

    return 0;
}
static int handle_setcoil(int sock, const char *input)
{
    key_t key = ftok("/home/hq/hqyj/IO/7-day/msg.c", 'a');
    if (key < 0)
    {
        perror("ftok err!");
        return -1;
    }
    else
    {
        printf("ftok success!,key: %#x\n", key);
    }
    int msgid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msgid <= 0)
    {
        if (errno == EEXIST)
        {
            printf("msg has set already, will open it directly!\n");
            msgid = msgget(key, 0666);
        }
        else
        {
            perror("msgget err!");
            return -1;
        }
    }
    printf("msg create success! msgid: %d\n", msgid);
    msg_t data;
    data.msgtype = 1;

    printf("set: %s\n", input);
    int value1, value2;
    sscanf(input, "set%d-%d", &value1, &value2);
    data.arr[0] = value1;
    data.arr[1] = value2;
    printf("setno1: %d setno2: %d\n", data.arr[0], data.arr[1]);
    msgsnd(msgid, &data, sizeof(data) - sizeof(long), 0);
    return 0;
}
static int handle_get(int sock, const char *input)
{   
    char reply_buf[HTML_SIZE] = {0};
    key_t key = ftok("/home/hq/hqyj/interpri/thttpd-master/main.c", 'a');
    if (key < 0)
    {
        perror("ftok err!");
        return -1;
    }
    else
    {
        printf("ftok success!\n");
    }
    printf("key: %d\n", key);
    int shmid = shmget(key, 4096, IPC_CREAT | IPC_EXCL | 0666);
    if (shmid <= 0)
    {
        if (errno == EEXIST)
        {
            printf("shm has set already, will open it directly!\n");
            shmid = shmget(key, 4096, 0666);
        }
        else
        {
            perror("shmget err!");
            return -1;
        }
    }
    else
        printf("shm create success!\n");
    
    u_int16_t *p = (u_int16_t *)shmat(shmid, NULL, 0);
    if (p == (u_int16_t *)-1)
    {
        perror("shmattach err!\n");
        return -1;
    }
    sprintf(reply_buf,"temp: %d&humi: %d\n", *p, *(p + 1));
    send(sock, reply_buf, strlen(reply_buf), 0);
    return 0;
}

/**
 * @brief 处理自定义请求，在这里添加进程通信
 * @param input
 * @return
 */
int parse_and_process(int sock, const char *query_string, const char *input)
{
    // query_string不一定能用的到

    // 先处理登录操作
    if (strstr(input, "username=") && strstr(input, "password="))
    {
        return handle_login(sock, input);
    }
    // 处理求和请求
    else if (strstr(input, "data1=") && strstr(input, "data2="))
    {
        return handle_add(sock, input);
    }
    else if (strstr(input, "set"))
    {
        return handle_setcoil(sock, input);
    }
    else if (strstr(input, "get"))
    {
        return handle_get(sock, input);
    }
    else // 剩下的都是json请求，这个和协议有关了
    {
        // 构建要回复的JSON数据
        const char *json_response = "{\"message\": \"Hello, client!\"}";

        // 发送HTTP响应给客户端
        send(sock, json_response, strlen(json_response), 0);
    }

    return 0;
}
