#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <modbus.h>
#include <modbus-tcp.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/msg.h>
#define SIZE 4096
typedef struct msg
{   
    long msgtype;
    int num;
    int arr[32];
}msg_t, *msg_p;
modbus_t *modb_init(const char *addr);

int dev, no;
modbus_t *ctx;
void *handler(void *arg)
{   
    key_t key = ftok("/home/hq/hqyj/IO/7-day/msg.c", 'a');
    if (key < 0)
    {
        perror("ftok err!");
        return NULL;
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
            return NULL;
        }
    }
    printf("msg create success! msgid: %d\n", msgid);
    msg_t data;
    while (1)
    {   
        msgrcv(msgid, &data, sizeof(data) - sizeof(long), 1, 0);
        printf("%d %d\n",data.arr[0],data.arr[1]);
        modbus_write_bit(ctx, data.arr[0], data.arr[1]);
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        printf("error type! argument format:\n\
                <executed file> <ip address>\n");
        return -1;
    }

    uint16_t dest[32] = {0};
    ctx = modb_init(argv[1]);
    pthread_t tid;
    pthread_create(&tid, NULL, handler, NULL);

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
    //  共享内存初始化
    int shmid = shmget(key, SIZE, IPC_CREAT | IPC_EXCL | 0666);
    if (shmid <= 0)
    {
        if (errno == EEXIST)
        {
            printf("shm has set already, will open it directly!\n");
            shmid = shmget(key, SIZE, 0666);
        }
        else
        {
            perror("shmget err!");
            return -1;
        }
    }
    else
        printf("shm create success!\n");

    uint16_t *p = (uint16_t *)shmat(shmid, NULL, 0);
    if (p == (uint16_t *)-1)
    {
        perror("shmattach err!\n");
        return -1;
    }
    while (1)
    {
        modbus_read_input_registers(ctx, 0x0000, 0x0002, dest);
        *p = dest[0];
        *(p + 1) = dest[1];
        // modbus_read_bits(ctx, 0x0000, 0x0002, dest);
        //printf("temp: %d  humi: %d\n", *p, *(p + 1));
        sleep(1);
    }

    shmdt(p);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

//  modbus初始化
modbus_t *modb_init(const char *addr)
{
    modbus_t *ctx;
    ctx = modbus_new_tcp(addr, 502);
    if (NULL == ctx)
    {
        printf("modbus_new_tcp err\n");
        return NULL;
    }
    if (modbus_set_slave(ctx, 0x01) < 0)
    {
        printf("modbus_set_slave err\n");
        return NULL;
    }
    if (modbus_connect(ctx) < 0)
    {
        printf("modbus_connect err\n");
        return NULL;
    }
    return ctx;
}
