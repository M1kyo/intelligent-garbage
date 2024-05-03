#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <error.h>
#include <Python.h>
#include <pthread.h>
#include <wiringPi.h>

#include "uartTool.h"
#include "garbage.h"
#include "pwm.h"
#include "oled_for_garbage.h"
#include "socket.h"

static int detect_process(const char* process_name)
{
    int n = -1;
    FILE* strm;
    char buf[128] = {0};
    sprintf(buf, "ps -ax | grep %s|grep -v grep", process_name);
    if ((strm = popen(buf, "r")) != NULL)
    {
        if (fgets(buf, sizeof(buf), strm) != NULL)
        {
            n = atoi(buf);
        }
    }else{
        return -1;
    }
    pclose(strm);
    return n;
}
#if 0
int main()
{
    int serial_fd = -1;
    int res_of_serial = 0;
    char* category = NULL;
    int process_ret = -1;
    unsigned char buffer[6] = {0xAA, 0x55, 0x00, 0x00, 0x55, 0xAA};
    
    garbage_init();
    process_ret = detect_process("mjpg_streamer");
    if (process_ret == -1){
        printf("mjpg_process error!\n");
        goto END;
    }
    serial_fd = myserialOpen(SERIAL_DEV, BAUD);
    if (serial_fd == -1)
    {
        printf("serial error!\n");
        goto END;
    }

    printf("entering while!\n");
    while(1)
    {
        res_of_serial = serialGetstring(serial_fd, buffer);

        printf("res_of_serial = %d\n", res_of_serial);
        
        if (res_of_serial > 0 && buffer[2] == 0x46)
        {
            buffer[2] = 0x00;
            system(WGET_CMD);
            if (access(GARBAGE_FILE, F_OK) == 0)
            {
                category = garbage_category(category);
                printf("category: %s", category);
                if (strstr(category, "干垃圾"))
                {
                    buffer[2] = 0x41;
                }
                else if (strstr(category, "湿垃圾"))
                {
                    buffer[2] = 0x42;
                }
                else if (strstr(category, "可回收垃圾"))
                {
                    buffer[2] = 0x43;
                }
                else if (strstr(category, "有害垃圾"))
                {
                    buffer[2] = 0x44;
                }else
                {
                    buffer[2] = 0x45;
                }
            }else{
                buffer[2] = 0x45;
                printf("access faied!\n");
            }
            printf("buffer[2] =%d\n", buffer[2]);
            serialSendstring(serial_fd, buffer, 6);
            if (buffer[2] == 0x43)
            {
                pwm_write(PWM_RECOVERABLE_GARBAGE);
                delay(2000);
                pwm_stop(PWM_RECOVERABLE_GARBAGE);
            }
            else if (buffer[2] != 0x45)
            {
                printf("lid open\n");
                pwm_write(PWM_GARBAGE);
                delay(2000);
                pwm_stop(PWM_GARBAGE);
            }
            buffer[2] = 0x00;
            remove(GARBAGE_FILE);
        }
    }
    close(serial_fd);
END:
    garbage_final();
    return 0;
}
#endif

int serial_fd = -1;
int res_of_serial =  -1;
pthread_cond_t cond;
pthread_mutex_t mutex;

void* pget_voice(void* arg)
{
    unsigned char buffer[6] = {0xAA, 0x55, 0x00, 0x00, 0x55, 0xAA};
    printf("%s|%s|%d|: pget_voice\n", __FILE__, __func__, __LINE__);
    if (serial_fd == -1)
    {
        printf("%s|%s|%d|: open serial failed!\n", __FILE__, __func__, __LINE__);
        pthread_exit(0);
    }
    printf("%s|%s|%d|: \n", __FILE__, __func__, __LINE__);
    while(1)
    {
        res_of_serial = serialGetstring(serial_fd, buffer);
        printf("%s|%s|%d|: res_of_serial\n", __FILE__, __func__, __LINE__);
        if (res_of_serial > 0 && buffer[2] == 0x46)
        {
            pthread_mutex_lock(&mutex);
            buffer[2] = 0x00;
            pthread_cond_signal(&cond);     //send a signal to pcategory(wait)
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_exit(0);
}

void* popen_trash(void* arg)
{

    pthread_detach(pthread_self());                       //父子线程分离
    unsigned char* buffer = (unsigned char*)arg;
    printf("buffer[2] =%d\n", buffer[2]);

    if (buffer[2] == 0x43)
    {
        pwm_write(PWM_RECOVERABLE_GARBAGE);
        delay(2000);
        pwm_stop(PWM_RECOVERABLE_GARBAGE);
    }
    else if (buffer[2] != 0x45)
    {
        printf("lid open\n");
        pwm_write(PWM_GARBAGE);
        delay(2000);
        pwm_stop(PWM_GARBAGE);
    }
    pthread_exit(0);
}

void* psend_voice(void* arg)
{
    pthread_detach(pthread_self());                       //父子线程分离

    unsigned char* buffer = (unsigned char*)arg;
    if (serial_fd == -1)
    {
        printf("%s|%s|%d|: open serial failed!\n", __FILE__, __func__, __LINE__);
        pthread_exit(0);
    }
    if (buffer != NULL){ 
        serialSendstring(serial_fd, buffer, 6);
    }
    pthread_exit(0);
}
void* poled_displaying(void* arg)
{
    pthread_detach(pthread_self());                       //父子线程分离
    myoled_init();
    oled_show(arg);
    pthread_exit(0);
}

void* pcategory(void* arg)
{
    char* category = NULL;
    unsigned char buffer[6] = {0xAA, 0x55, 0x00, 0x00, 0x55, 0xAA};
    pthread_t send_voice_tid, trash_tid, oled_tid;
    printf("%s|%s|%d|: pcategory\n", __FILE__, __func__, __LINE__);
    while (1)
    {
        printf("%s|%s|%d|: pcategory mutex\n", __FILE__, __func__, __LINE__);
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);     //send a signal to pcategory(wait)
        pthread_mutex_unlock(&mutex);

        printf("%s|%s|%d|: pcategory mutex unlock\n", __FILE__, __func__, __LINE__);
        
        buffer[2] = 0x00;
        
        system(WGET_CMD);                     //take a photo~
        if (access(GARBAGE_FILE, F_OK) == 0)
        {
            category = garbage_category(category);
            printf("category: %s", category);
            if (strstr(category, "干垃圾"))
            {
                buffer[2] = 0x41;
            }
            else if (strstr(category, "湿垃圾"))
            {
                buffer[2] = 0x42;
            }
            else if (strstr(category, "可回收垃圾"))
            {
                buffer[2] = 0x43;
            }
            else if (strstr(category, "有害垃圾"))
            {
                buffer[2] = 0x44;
            }else
            {
                buffer[2] = 0x45;
            }
        }else{
            buffer[2] = 0x45;
            printf("access faied!\n");
        }
        //==================================key part===================================
        
        //  opening a lid    pthread:
        printf("%s|%s|%d|: opening a lid    pthread\n", __FILE__, __func__, __LINE__);
        pthread_create(&trash_tid, NULL, popen_trash, (void*)buffer);
        
        //  sending voice    pthread:
        printf("%s|%s|%d|: sending voice    pthread\n", __FILE__, __func__, __LINE__);
        pthread_create(&send_voice_tid, NULL, psend_voice, (void*)buffer);

        //oled displaying    pthread:
        printf("%s|%s|%d|: oled displaying    pthread\n", __FILE__, __func__, __LINE__);
        pthread_create(&oled_tid, NULL, poled_displaying, (void*)buffer);   

        remove(GARBAGE_FILE);                   //delete the photo
    }
    pthread_exit(0);   
}
void* pget_socket(void* arg)
{
    int s_fd = -1;
    int c_fd = -1;
    char buffer[6];
    int nread = -1;
    struct sockaddr_in c_addr;
    
    memset(&c_addr,0,sizeof(struct sockaddr_in));

    s_fd = socket_init(IPADDR, IPPORT);
    printf("%s|%s|%d:s_fd=%d\n", __FILE__, __func__, __LINE__, s_fd); 
    if (s_fd == -1)
    {
        pthread_exit(0);
    }

    sleep(3);
    int clen = sizeof(struct sockaddr_in);
    
    while (1)
    {
        c_fd = accept(s_fd,(struct sockaddr *)&c_addr,&clen);
        int keepalive = 1;          // 开启TCP KeepAlive功能
        int keepidle = 5;        // tcp_keepalive_time 3s内没收到数据开始发送心跳包
        int keepcnt = 3;            // tcp_keepalive_probes 每次发送心跳包的时间间隔,单位秒

        int keepintvl = 3;         // tcp_keepalive_intvl 每3s发送一次心跳包
        setsockopt(c_fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive));
        setsockopt(c_fd, SOL_TCP, TCP_KEEPIDLE, (void *) &keepidle, sizeof (keepidle));
        setsockopt(c_fd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcnt, sizeof (keepcnt));
        setsockopt(c_fd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepintvl, sizeof (keepintvl));
        printf("%s|%s|%d: Accept a connection from %s:%d\n", __FILE__, __func__, __LINE__, 
                                        inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));
        if(c_fd == -1)
        {
            perror("accept");
            continue;
        }
        while(1)
        {
            memset(buffer, 0, sizeof(buffer));
            nread =  recv(c_fd, buffer, sizeof(buffer), 0); //n_read = read(c_fd, buffer, sizeof(buffer));
            printf("%s|%s|%d:nread=%d, buffer=%s\n", __FILE__, __func__, __LINE__, nread, buffer);
            if (nread > 0)
            {
                if (strstr(buffer, "open"))
                {
                    pthread_mutex_lock(&mutex);
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                }
            }
            else if(nread == 0 || nread == -1)
            {
                break;
            }
        }
        close(c_fd);
    }
    pthread_exit(0);   
}
int main()
{
    int res_of_serial = 0;
    char* category = NULL;
    int process_ret = -1;
    unsigned char buffer[6] = {0xAA, 0x55, 0x00, 0x00, 0x55, 0xAA};
    pthread_t get_voice_tid, category_tid, get_socket_tid;

    wiringPiSetup();
    garbage_init();

    process_ret = detect_process("mjpg_streamer");
    if (process_ret == -1){
        printf("mjpg_process error!\n");
        goto END;
    }

    serial_fd = myserialOpen(SERIAL_DEV, BAUD);
    if (serial_fd == -1)
    {
        printf("serial error!\n");
        goto END;
    }
    //==================================key part===================================

    //  voice pthread:
    pthread_create(&get_voice_tid, NULL, pget_voice, NULL);
    
    //  aliyun pthread:
    pthread_create(&category_tid, NULL, pcategory, NULL);
    
    //  socket pthread:
    pthread_create(&get_socket_tid, NULL, pget_socket, NULL);
    
    //  waiting the end of pthread
    pthread_join(get_voice_tid, NULL);
    pthread_join(category_tid, NULL);
    pthread_join(get_socket_tid, NULL);


    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    close(serial_fd);
END:
    garbage_final();
    return 0;
}