#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX 4096
#define NUM 100
sem_t semlock;
struct state {
    int clientsock;
    char search_key[MAX];
};

typedef struct {
    char *name;
    char *key;
    int   start;
    int   end;
    char *final;
    sem_t *lock;
} thread;

void handler(int x) {
    while (wait(NULL) > 0) { }
}

int size(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fclose(fp);
    return (int)len;
}

void checK_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "error (opening file '%s' for checking)\n", filename);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
}

int include_key(const char *paragraph, const char *key) {
    if (!paragraph || !key || strlen(key) < 2) return 0;
    char *p = strstr(paragraph, key);
    while (p) {
        if ((p == paragraph || !isalpha((unsigned char)p[-1])) &&
            !isalpha((unsigned char)p[strlen(key)]))
            return 1;
        p = strstr(p + 1, key);
    }
    return 0;
}
/*int include_key(const char *paragraph, const char *key) {
    if (!key || strlen(key) == 0) return 0;
    return (strstr(paragraph, key) != NULL);
}
*/
/*
int include_key(const char *paragraph, const char *key){
        if(!key || strlen(key) < 2) return 0;
        char *p = strstr(paragraph, key);
        while (p){
                if((p == paragraph || !isalpha((p - 1))) && !isalpha((p + strlen(key)))) return 1;
                p = strstr(p + 1, key);

        //      printf("-------\n");
        }
        return 0;
}*/
void *search_file(void *ar) {
    thread *args=(thread *)ar;
    FILE *fp=fopen(args->name, "r");
    if (!fp)
            return NULL;
    fseek(fp,args->start,SEEK_SET);
    int x=size(args->name);
    char *buf=malloc(x);
    char *paragraph=calloc(x, 1);
    int pos=args->start;
    while (pos<args->end&&fgets(buf,x,fp)) {
           // if(pos>=args->end)
        //          break;
        pos=ftell(fp);
        if (strcmp(buf, "\n")==0){
            if (include_key(paragraph,args->key)) {
                sem_wait(args->lock);
                strcat(args->final,paragraph);
                strcat(args->final,"\n");
                sem_post(args->lock);
            }
            paragraph[0]='\0';
        } else {
            if (strlen(paragraph)+strlen(buf)<x-1)
                strcat(paragraph,buf);
        }
        if (pos>=args->end)
                break;
    }
    /* catch the last paragraph if no trailing blank line */
    if (include_key(paragraph,args->key)) {
        sem_wait(args->lock);
        strcat(args->final,paragraph);
        strcat(args->final,"\n");
        sem_post(args->lock);
    }
    free(buf);
    free(paragraph);
    fclose(fp);
    //free(args);
    return NULL;
}
void *search_in_parts(char *name1,char *key1,char *result1,sem_t *lock1) {
    int len=size(name1);
    if (len<=0)
            return NULL;
    thread args[NUM];
    pthread_t th[NUM];
    int chunk=len/NUM;
    int presize=strlen(result1);
    for (int i=0;i<NUM;i++){
        args[i].name=name1;
        args[i].key=key1;
        args[i].start=i*chunk;
        if (i==NUM-1)
                args[i].end=len;
        else
                args[i].end=(i+1)*chunk;
        args[i].final=result1;
        args[i].lock =lock1;
        pthread_create(&th[i],NULL,search_file,&args[i]);
    }
    for (int i=0;i<NUM;i++) {
       pthread_join(th[i],NULL);
    }
    if (strlen(result1)>presize){
        char *hdr=malloc(MAX);
        snprintf(hdr,MAX,"FOUND in: %s\n",name1);
        char *body=result1+presize;
        char *tmp=malloc(strlen(hdr)+strlen(body) + 1);
        strcpy(tmp,hdr);
        strcat(tmp,body);
        strcpy(body,tmp);
        free(tmp);
        free(hdr);
    }
    else {
        char hdr[MAX];
        snprintf(hdr,sizeof(hdr),"NOT FOUND in: %s\n",name1);
        strcat(result1, hdr);
    }
    strcat(result1, "----------------------------\n");
    return NULL;
}
int main(int argc , char *argv[]){

        signal(SIGCHLD, handler);
        if(argc !=2){
                fprintf(stderr, "(Too little argumnets) , Hint: Needs a PORT number\n");
                exit(EXIT_FAILURE);
        }
        int PORT = atoi(argv[1]);


        checK_file("text1.txt");
        checK_file("text2.txt");
        checK_file("text3.txt");

        int serverfd;
        struct sockaddr_in serveraddr;
        socklen_t addrlen = sizeof(serveraddr);
        char buf[MAX] = {0};
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if(serverfd == 0) {
                perror("error (socket)");
                exit(EXIT_FAILURE);
        }
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serveraddr.sin_port = htons(PORT);
        if(bind(serverfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0){
                perror("error (bind)");
                close(serverfd);
                exit(EXIT_FAILURE);
        }
        if(listen(serverfd, 5) < 0){
                perror("error (listen)");
                close(serverfd);
                exit(EXIT_FAILURE);
        }
        printf("The Search Server is running on port %d ... \n", PORT);
        while (1){
                struct sockaddr_in clientaddr;
                int clientsock = accept(serverfd, (struct sockaddr *)&clientaddr, &addrlen);
                if(clientsock < 0){
                        perror("error (accept)");
                        close(serverfd);
                        exit(EXIT_FAILURE);
                }
        //      printf("-------\n");
                pid_t pid = fork();
                if(pid == 0){
                close(serverfd);
                int read_bytes;
                 int x=size("text2.txt");
                        int x1=size("text1.txt");
                        int x2=size("text3.txt");
                        char *finalresult=calloc(x+x1+x2+1000000+x2,1);
                        if(!finalresult){
                        printf("no no \n");
                        exit(0);
                        }
                while((read_bytes = read(clientsock, buf, MAX - 1)) > 0){
                        buf[read_bytes] = '\0';
                        char *key = strtok(buf, "\n");

                        if(!key || strlen(key) == 0 || strcmp(key , "\n") == 0){
                                printf("Received empty or invalid search key.\n");
                                char * msg = "Received an empty key . Please enter a valid keyword .\n";
                                send(clientsock , msg  , strlen(msg) , 0);
                                break;
                        }
                        /*int x=size("The Project Gutenberg eBook of Alic.txt");
                        int x1=size("text1.txt");
                        int x2=size("text3.txt");
                        char finalresult=calloc(x+x1+x2+100,1);*/
                        //sem_t semlock;
                        sem_init(&semlock,0,1);
                        printf("Received search key: %s\n", key);
                        struct state s1;
                        s1.clientsock = clientsock;
                        strncpy(s1.search_key, key, MAX);
                //      const char *target_file = "text1.txt";

                        search_in_parts("text1.txt",key,finalresult,&semlock);
                        search_in_parts("text2.txt",key,finalresult,&semlock);
                        search_in_parts("text3.txt",key,finalresult,&semlock);

                        send(clientsock,finalresult,strlen(finalresult), 0);
                    free(finalresult);
                }
//              send(clientsock,finalresult,strlen(finalresult), 0);
                    //free(finalresult);
                close(clientsock);
                exit(0);
                }else{
                        close(clientsock);
                }
        }
        close(serverfd);
        return 0;
}