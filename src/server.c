#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 1024

#define HINT 3

char* hangman(int num);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    char msg[BUF_SIZE];
    int str_len;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    fd_set reads, reads_;
    struct timeval timeout;
    int fd_max, fd_num, i;

    int user_cnt = 0;        //user count
    int user[2] = { 0 };    //user정보 = [소켓]
    int gameOK = 0;    //game 진행 가능 여부
    int resolver[2] = { 0 };    //플레이어 [0] = 소켓 [1] = 플래그
    int exam[2] = { 0 };    //출제자 [0] = 소켓 [1] = 플래그
    char answer[BUF_SIZE];    //출제답안
    char question[BUF_SIZE];    //맞추고있는단어
    int hint_cnt, hang_cnt, j, iscorrect, iscontinue;//힌트 개수, 매달린 개수, 정답 유무

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == -1) {
        perror("socket Error!");
        exit(1);
    }
    //bind()
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind Error");
        exit(1);
    }

    if (listen(serv_sock, 5) == -1) {
        perror("listen error");
        exit(1);
    }

    clnt_addr_size = sizeof(clnt_addr);

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    fd_max = serv_sock;

    while (1)
    {
        reads_ = reads;    //문제가 바뀔 수 있기 때문에 복사하여 사용
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;    //microsec
        fd_num = select(fd_max + 1, &reads_, 0, 0, &timeout);

        if (fd_num == -1) break;    //오류 >> 종료
        if (fd_num == 0) continue;    //소켓x >> 계속

        for (i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &reads_))    //1 or ?1
            {
                if (i == serv_sock)    //listening소켓 확인
                {    //accept
                    clnt_sock = accept(serv_sock, (struct sockaddr*) & clnt_addr, &clnt_addr_size);
                    if (clnt_sock == -1) {
                        perror("accept Error");
                        exit(1);
                    }
                    FD_SET(clnt_sock, &reads);
                    if (clnt_sock > fd_max)
                        fd_max = clnt_sock;
                    printf("Client connect\n");
                    user_cnt++;
                    if (user_cnt <= 2) {
                        user[user_cnt - 1] = clnt_sock;
                        strcpy(msg, "--Welcome to HangMan Game--\n");
                        if (user_cnt == 1) {
                            strcat(msg, "waiting for another player.....\n");
                        }
                        else if (FD_ISSET(user[0], &reads) && user_cnt == 2) {
                            strcat(msg, "Game will be start...\n");
                            send(user[0], "Game will be start...\n", strlen("Game will be start...\n"), 0);
                            gameOK = 1;
                        }
                        send(clnt_sock, msg, strlen(msg), 0);
                    }
                    else {    //플레이어 2명이 준비되었을 때
                        send(clnt_sock, "fail", strlen("fail"), 0);
                    }

                    if (FD_ISSET(user[0], &reads) && FD_ISSET(user[1], &reads) && gameOK == 1 && user_cnt == 2) {    //게임 시작
                        hint_cnt = HINT; hang_cnt = 0;
                        strcpy(msg, "Question for ");
                        send(user[0], msg, strlen(msg), 0);
                        //난수생성 
                        srand(time(NULL));
                        exam[0] = user[rand() % 2];
                        if (exam[0] == user[0])
                            resolver[0] = user[1];
                        else
                            resolver[0] = user[0];
                        strcpy(msg, "the another player\n");
                        send(resolver[0], msg, strlen(msg), 0);
                        strcpy(msg, "waiting for another Player\n");
                        send(resolver[0], msg, strlen(msg), 0);
                        strcpy(msg, "Question for You\n");
                        send(exam[0], msg, strlen(msg), 0);
                        strcpy(msg, "Enter Question in English LOWER : ");
                        send(exam[0], msg, strlen(msg), 0);
                        exam[1] = 1;
                    }
                }
                else
                {    //client >> receive
                    str_len = recv(i, msg, BUF_SIZE, 0);

                    if (str_len == 0)
                    {    //client 종료
                        FD_CLR(i, &reads);
                        close(i);
                        printf("Client Left");
                        user_cnt--;
                        if (user_cnt == 1 && gameOK == 1) { //한명만남았을시 gameOK변경 >> 게임 시작 불가
                            strcpy(msg, "\nPlayer LEFT...\n");
                            resolver[1] = 0;
                            exam[1] = 0;
                            if (user[0] == i) {
                                strcat(msg, "Player LEFT...\n");
                                strcat(msg, "Waiting For Player...\n");
                                user[0] = user[1];
                                send(user[1], msg, strlen(msg), 0);
                            }
                            else if (FD_ISSET(user[0], &reads)) {
                                send(user[0], msg, strlen(msg), 0);
                            }
                            gameOK = 0;
                        }
                    }
                    else if (i == resolver[0] && FD_ISSET(exam[0], &reads) && gameOK == 1)
                    {
                        msg[str_len - 1] = 0;
                        if (strcmp(msg, ""))
                            switch (resolver[1]) {
                            case 1:
                                if (!strcmp(msg, "hint")) {    //힌트요청
                                    if (hint_cnt == 0) {    //사용 가능 힌트가 없으면
                                        strcpy(msg, "No Hint for You now\n");
                                        strcat(msg, "Enter : ");
                                        send(i, msg, strlen(msg), 0);
                                    }
                                    else {
                                        strcpy(msg, "Player wants hint for U...\n");
                                        strcat(msg, "Hint Enter : ");
                                        send(exam[0], msg, strlen(msg), 0);
                                        exam[1] = 3;
                                        resolver[1] = 0;
                                    }
                                    break;
                                }
                                iscontinue = 0; iscorrect = 0;
                                if (strlen(msg) == 1) {//알파벳 확인 
                                    for (j = 0; j < strlen(answer); j++) {
                                        if (answer[j] == msg[0]) {
                                            iscorrect = 1;
                                            question[j] = answer[j];
                                        }
                                        else if (question[j] == '*')
                                            iscontinue = 1;
                                    }
                                    strcat(msg, "\n");
                                    send(exam[0], "Answer : ", strlen("Answer : "), 0);
                                    send(exam[0], msg, strlen(msg), 0);

                                    if (iscorrect == 1)    //정답
                                        strcpy(msg, "Great!\n");
                                    else {            //실패
                                        strcpy(msg, "No alphabet in Question!\n");
                                        hang_cnt++;
                                    }    //결과
                                    strcat(msg, "- Question : ");
                                    strcat(msg, question); strcat(msg, "\n");
                                    strcat(msg, hangman(hang_cnt));
                                    send(exam[0], msg, strlen(msg), 0);
                                    send(resolver[0], msg, strlen(msg), 0);
                                    if (iscontinue == 0) {    //모두 맞출 경우
                                        strcpy(msg, "Great! Player Win!\n");
                                    }
                                    else if (hang_cnt == 7) {    //Life X
                                        strcpy(msg, "No LIFE.. GG\n");
                                    }
                                    else {    //Life O>입력 다시 받음 
                                        strcpy(msg, "Player Writing Answer...\n");
                                        send(exam[0], msg, strlen(msg), 0);
                                        strcpy(msg, "If you want hint input hint\n");
                                        send(resolver[0], msg, strlen(msg), 0);
                                        break;
                                    }    //게임 종료(플레이어가 남아있으면 계속 진행)
                                    strcat(msg, "Game End...\n");
                                    strcat(msg, "Now Question For ");
                                    send(exam[0], msg, strlen(msg), 0);
                                    send(resolver[0], msg, strlen(msg), 0);
                                    //게임 재시작
                                    hint_cnt = HINT; hang_cnt = 0;
                                    resolver[0] = exam[0];
                                    exam[0] = i;
                                    strcpy(msg, "Another Player!\n");
                                    strcat(msg, "Waiting For Question!\n");
                                    send(resolver[0], msg, strlen(msg), 0);
                                    strcpy(msg, "YOU!\n");
                                    strcat(msg, "Question for LOWER Alphabet : ");
                                    send(exam[0], msg, strlen(msg), 0);
                                    exam[1] = 1;
                                    resolver[1] = 0;
                                    break;
                                }
                            default:
                                break;
                            }
                    }
                    else if (i == exam[0] && FD_ISSET(resolver[0], &reads) && gameOK == 1)
                    {
                        msg[str_len - 1] = 0;
                        if (strcmp(msg, ""))
                            switch (exam[1]) {
                            case 1:
                                strcpy(answer, msg);
                                printf("%s\n", answer);
                                strcpy(msg, "Question YOU Write : ");
                                strcat(msg, answer);
                                strcat(msg, "\nYes('y') / No('n') : ");
                                send(i, msg, strlen(msg), 0);
                                exam[1] = 2;
                                break;
                            case 2:    //출제 단어 재확인
                                if (!strcmp(msg, "y")) {
                                    exam[1] = 0;
                                    resolver[1] = 1;
                                    for (j = 0; j < strlen(answer); j++)
                                        question[j] = '*';
                                    question[j] = '\0';

                                    strcpy(msg, "- Question : ");
                                    strcat(msg, question); strcat(msg, "\n");
                                    send(exam[0], msg, strlen(msg), 0);
                                    send(resolver[0], msg, strlen(msg), 0);

                                    strcpy(msg, hangman(0));
                                    send(exam[0], msg, strlen(msg), 0);
                                    send(resolver[0], msg, strlen(msg), 0);

                                    strcpy(msg, "Player Writing Answer...\n");
                                    send(exam[0], msg, strlen(msg), 0);
                                    strcpy(msg, "If you want hint input hint\n ");
                                    send(resolver[0], msg, strlen(msg), 0);
                                }
                                else if (!strcmp(msg, "n")) {
                                    strcpy(msg, "Question for LOWER ALPHABET : ");
                                    send(exam[0], msg, strlen(msg), 0);
                                    exam[1] = 1;
                                }
                                else {
                                    strcpy(msg, "Write Again.\n yes('y') / no('n') : ");
                                    send(exam[0], msg, strlen(msg), 0);
                                }
                                break;
                            case 3:    //힌트요청
                                strcat(msg, "\n");
                                send(resolver[0], msg, strlen(msg), 0);
                                hint_cnt--;
                                strcpy(msg, "Player writing Answer.\n");
                                send(exam[0], msg, strlen(msg), 0);
                                strcpy(msg, "Low Alphabet plz : ");
                                send(resolver[0], msg, strlen(msg), 0);
                                exam[1] = 0;
                                resolver[1] = 1;
                                break;
                            default:
                                break;
                            }
                    }
                }
            }
        }
    }

    close(serv_sock);
    return 0;
}


char* hangman(int num) {//행맨을 그리는 작업
    char* hangman = { 0 };
    switch (num)
    {
    case 0:
        hangman = "┌───┐\n│\n│\n│\n│\n└──────\n";
        break;
    case 1:
        hangman = "┌───┐\n│ 　○\n│\n│\n│\n└──────\n";
        break;
    case 2:
        hangman = "┌───┐\n│ 　○\n│　 |\n│\n│\n└──────\n";
        break;
    case 3:
        hangman = "┌───┐\n│ 　○\n│　/|\n│\n│\n└──────\n";
        break;
    case 4:
        hangman = "┌───┐\n│ 　○\n│　/|\\\n│　\n│\n└──────\n";
        break;
    case 5:
        hangman = "┌───┐\n│ 　○\n│　/|\\\n│　/\n│\n└──────\n";
        break;
    case 6:
        hangman = "┌───┐\n│ 　○\n│　/|\\\n│  / \\\n│\n└──────\n";
        break;
    case 7:
        hangman = "┌───┐\n│ 　○\n│　(DIE)\n│　/|\\\n│  / \\\n└──────\n";
        break;
    default:
        hangman = "drawing error\n";
        break;
    }
    return hangman;
}

