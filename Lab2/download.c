#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <regex.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"

void getURL(char *username, char *password, char *returnHost, char *file, char *argv);
struct hostent *getHostname(char *host);
int parseStartConnection(char* buf);
int sendUserPass(int sockfd, char * user,char * pass);
void getFile(int sockfd,int sockfdData,char * file);

// download ftp://[<user>:<password>@]ftp.up.pt/pub

int main(int argc, char **argv)
{
    struct hostent *h;
    int sockfd;
    int sockfdData;
    struct sockaddr_in server_addr;

    if (argc != 2)
    {
        perror("usage : ./download ftp://[url]");
        exit(1);
    }

    char *username = malloc(sizeof(char) * 256);
    char *password = malloc(sizeof(char) * 256);
    char *file = malloc(sizeof(char) * 256);
    char *host = malloc(sizeof(char) * 256);
    getURL(username, password, host, file, argv[1]);

    printf("user: %s\n", username);
    printf("pass: %s\n", password);
    printf("host: %s\n", host);
    printf("file: %s\n", file);

    h = getHostname(host);

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr))); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(SERVER_PORT);                                          /*server TCP port must be network byte ordered */

    /*open an TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(0);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(0);
    }

    char buf[256];
    int bytesRead = 0;
    int readStatus = 1;
    while (readStatus)
    {
        bytesRead = read(sockfd, buf, 256);
        printf("\t");
        fflush(stdout);
        write(1, buf,bytesRead);
        readStatus = parseStartConnection(buf);
        buf[0] = '\0';
    }

    printf("parsed starting connection\n");

    int serverPort = sendUserPass(sockfd,username,password);
    if(serverPort == -1){
        printf("failed when receiving port from server");
    }

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr))); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(serverPort);                                          /*server TCP port must be network byte ordered */

    /*open an TCP socket*/
    if ((sockfdData = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(0);
    }
    /*connect to the server*/
    if (connect(sockfdData,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(0);
    }

    printf("connected to data server-port\n");
    printf("getting file:%s\n",file);

    getFile(sockfd,sockfdData,file);


    free(username);
    free(password);
    free(host);
    free(file);
}

int parseStartConnection(char* buf){
    char status[4];
    strncpy(status,buf,3);
    fflush(stdout);
    printf("testing :%s\n",status);
    if(strcmp(status,"220")){
        printf("wrong message received, not code 220");
        exit(1);
    }
    if(buf[3] != '-')
        return 0;
    return 1;
}

void getFile(int sockfd,int sockfdData,char * file){
    char * cmd = malloc(sizeof(char)*50);
    int bytesRead = 0;
    cmd[0] = '\0';
    strcat(cmd,"retr ");
    strcat(cmd,file);
    strcat(cmd,"\n");
    write(sockfd,cmd,strlen(cmd));
    char * buf = malloc(sizeof(char)*256);
    bytesRead = read(sockfd,buf,256);
    printf("\t");
    fflush(stdout);
    write(1,buf,bytesRead);
    if(strncmp(buf,"150",3)){
        printf("couldn't open binary data connection for this file\n");
    }
    else
    {
        char *token;
        token = strtok(buf, "(");
        token = strtok(NULL,")");
        char * aux_token;
        aux_token = strtok(token," ");
        int bytesToRead = atoi(aux_token);

        char * auxFile;
        char * lastToken;
        auxFile = strtok(file,"/");
        while(auxFile != NULL){
            lastToken = auxFile;
            auxFile = strtok(NULL,"/");
        }

        FILE * fileToCreate = fopen(lastToken,"wb");
        if(fileToCreate == NULL){
            printf("error opening file\n");
            exit(1);
        }
        char fileBuffer[bytesToRead];
        char auxBuf[256];
        bytesRead = 0;
        int totalBytesRead = 0;
        while(totalBytesRead < bytesToRead){
            bytesRead = read(sockfdData,auxBuf,256);
            memcpy(fileBuffer+totalBytesRead,auxBuf,bytesRead);
            totalBytesRead += bytesRead;
        }

        fwrite(fileBuffer,sizeof(char),bytesToRead,fileToCreate);
        fclose(fileToCreate);
        printf("Finished creating file\n");

    }
}

int sendUserPass(int sockfd, char * user,char * pass){
    char * cmd = malloc(sizeof(char)*50);
    int bytesRead = 0;
    cmd[0] = '\0';
    strcat(cmd,"user ");
    strcat(cmd,user);
    strcat(cmd,"\n");
    write(sockfd,cmd,strlen(cmd));
    char * buf = malloc(sizeof(char)*256);
    bytesRead = read(sockfd,buf,256);
    printf("\t");
    fflush(stdout);
    write(1,buf,bytesRead);
    if(strncmp(buf,"331",3)){
        printf("couldn't specify password\n");
    }
    else
    {
        cmd[0] = '\0';
        strcat(cmd,"pass ");
        strcat(cmd,pass);
        strcat(cmd,"\n");

        write(sockfd,cmd,strlen(cmd));
        bytesRead = read(sockfd,buf,256);

        printf("\t");
        fflush(stdout);
        write(1,buf,bytesRead);
    }
    if(strncmp(buf,"230",3)){
        printf("didn't login successfully\n");
    }
    else{
        cmd[0] = '\0';
        strcat(cmd,"pasv\n");
        write(sockfd,cmd,strlen(cmd));
        bytesRead = read(sockfd,buf,256);

        printf("\t");
        fflush(stdout);
        write(1,buf,bytesRead);
    }
    if(strncmp(buf,"227",3)){
        printf("couldn't enter passive mode\n");
    }
    else{
        char *token;
        token = strtok(buf, "(");
        token = strtok(NULL,")");
        char * aux_token;
        aux_token = strtok(token,",");
        aux_token = strtok(NULL,",");
        aux_token = strtok(NULL,",");
        aux_token = strtok(NULL,",");
        aux_token = strtok(NULL,",");
        int port;
        int aux;
        port = atoi(aux_token)*256;
        aux_token = strtok(NULL,",");
        aux = atoi(aux_token);
        port += aux;
        return port;
    }
    return -1;
    
}

void getURL(char *username, char *password, char *returnHost, char *file, char *argv)
{
    regex_t regex;
    if (regcomp(&regex, "^ftp://((a-zA-Z0-9)+:(a-zA-Z0-9)*@)?.+(/[^/]+)+/?+$", REG_EXTENDED) != 0)
    {
        perror("Url regex");
        exit(1);
    }

    int match = regexec(&regex, argv, 0, NULL, 0);
    if (match == REG_NOMATCH)
    {
        puts("invalid url");
        free(username);
        free(password);
        free(returnHost);
        free(file);
        exit(1);
    }

    regfree(&regex);

    char *token;
    token = strtok(argv, "/");
    token = strtok(NULL, "/"); //Skip ftp://
    int isHost = 0;
    char *testHost = malloc(sizeof(char) * 50);
    char *filepath = malloc(sizeof(char) * 256);
    strcpy(filepath, "");
    while (token != NULL)
    {
        if (isHost == 0)
        {
            strcpy(testHost, token);
            isHost = 1;
        }
        else
        {
            strcat(filepath, token);
            strcat(filepath, "/");
        }
        token = strtok(NULL, "/");
    }

    char *pPosition = strchr(testHost, '@');
    char *user = malloc(sizeof(char) * 50);
    char *pass = malloc(sizeof(char) * 50);
    char *host = malloc(sizeof(char) * 50);

    if (pPosition == NULL)
    {
        strcpy(user, "anonymous");
        strcpy(pass, "");
        strcpy(host, testHost);
    }
    else
    {
        char *pointsPosition = strchr(testHost, ':');
        if (pointsPosition == NULL)
        {
            perror("No division between the user and password\n");
            exit(1);
        }

        const char dividePass = '@';
        const char divideUser = ':';

        user = strtok(testHost, &divideUser);
        pass = strtok(NULL, &dividePass);
        host = strtok(NULL, &dividePass);

        if (user == NULL || pass == NULL || host == NULL)
        {
            perror("Please use the following format for the introduction of username and password : username:password@host\n");
            exit(1);
        }
    }

    strcpy(username, user);
    strcpy(password, pass);
    strcpy(file, filepath);
    file[strlen(file) - 1] = '\0'; // remove '/' from the end of the path
    strcpy(returnHost, host);
    //free(user);
    //free(pass);
    //free(host);
    //free(filepath);
}

struct hostent *getHostname(char *host)
{
    struct hostent *h;
    if ((h = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }

    return h;
}
