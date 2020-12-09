#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024

void* send_msg(void* arg);
void* recv_msg(void* arg);

int main(int argc, char* argv[])
{
    int sock;
    char msg[BUF_SIZE];
    int str_len, recv_len, recv_cnt;
    struct sockaddr_in serv_addr;
    pthread_t st, rt;
    void* thread_return;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        perror("SOCK ERROR");
        exit(1);
    }
    //bind()
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connect Error");
        exit(1);
    }
    else
        puts("Connected...........");

    pthread_create(&st, NULL, send_msg, (void*)&sock);
    pthread_create(&rt, NULL, recv_msg, (void*)&sock);

    pthread_join(st, &thread_return);
    pthread_join(rt, &thread_return);

    close(sock);
    return 0;
}

void* send_msg(void* arg)
{
    int sock = *((int*)arg);
    int str_len;
    char msg[BUF_SIZE];

    while (1)
    {
        fgets(msg, BUF_SIZE, stdin);
        str_len = write(sock, msg, strlen(msg));
    }
}

void* recv_msg(void* arg)
{
    int sock = *((int*)arg);
    int recv_cnt;
    char msg[BUF_SIZE];

    while (1)
    {
        recv_cnt = read(sock, &msg, BUF_SIZE - 1);
        if (recv_cnt == -1)
            perror("Read Error");

        msg[recv_cnt] = 0;
        if (!strcmp(msg, "fail"))
            perror("Game Start Error");
        printf("%s", msg);
    }
}
