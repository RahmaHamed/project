#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#define BUF_SIZE 5000
void handler(int x) {
    printf("\nThe program has stopped. Thanks for using it\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("put the IP machine and the port\n");
        exit(1);
    }

    signal(SIGINT, handler);
    char *name = argv[1];
    int port = atoi(argv[2]);
    while (1) {
        char keyword[256];
        printf("Enter search keyword: ");
        if (!fgets(keyword, sizeof(keyword), stdin)) {
            printf("Input error\n");
            break;
        }

        int len = strlen(keyword);
        if (len > 0&&keyword[len - 1] != '\n') {
            keyword[len]='\n';
            keyword[len+1] = '\0';
        }
        if (strlen(keyword) <= 1) {
            printf("Empty keyword. Try again.\n");
            continue;
        }
        int fd=socket(AF_INET,SOCK_STREAM, 0);
        if (fd<0) {
            perror("socket");
            exit(1);
        }
        struct sockaddr_in sock;
        memset(&sock,0,sizeof(sock));
        sock.sin_family=AF_INET;
        sock.sin_port=htons(port);

        if (inet_pton(AF_INET, name, &sock.sin_addr) <= 0) {
            printf("error \n");
        }
        if (connect(fd,(struct sockaddr *)&sock,sizeof(sock))<0) {
            perror("connect");
            close(fd);
            continue;
        }
        if (write(fd,keyword,strlen(keyword))<0) {
            perror("write");
            close(fd);
            continue;
        }
        char buffer[BUF_SIZE];
        int rd;
        printf("Results:\n");
        while ((rd=read(fd,buffer,BUF_SIZE-1))>0) {
            buffer[rd]='\0';
            printf("%s",buffer);
        }
        if (rd<0) {
            perror("read");
        }

        printf("\n");
        close(fd);
    }

    return 0;
}
