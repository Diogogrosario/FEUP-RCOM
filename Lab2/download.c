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

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"

void getURL(char * url, char * argv);
struct hostent *getHostname(char *host);

// download ftp://[<user>:<password>@]ftp.up.pt/pub

// 

int main(int argc, char **argv){

    if(argc != 2){
        perror("usage : ./download ftp://[url]");
        exit(1);
    }

    char * url = malloc(sizeof(char)*256);
    getURL(url,argv[1]);


    free(url);
}


void getURL(char * url, char * argv){
  regex_t regex;
  if (regcomp(&regex, "^ftp://((a-zA-Z0-9)+:(a-zA-Z0-9)*@)?.+(/[^/]+)+$", REG_EXTENDED) != 0) {
    perror("Url regex");
    exit(1);
  }

  int match = regexec(&regex, argv, 0, NULL, 0);
  if (match == REG_NOMATCH){
    puts("invalid url");
    free(url);
    exit(1);
  }

  regfree(&regex);

    const char delimiter = '/';
    char *token;
    token = strtok(argv,&delimiter);
    token = strtok(NULL,&delimiter); //Skip ftp://
    int isHost = 0;
    char * host = malloc(sizeof(char)*50);
    char * filepath = malloc(sizeof(char)*256);
    while(token != NULL){
        if(isHost == 0){
            strcpy(host,token);
            isHost = 1;
        }
        else{
            strcat(filepath,token);
            strcat(filepath,"/");
        }
        token = strtok(NULL,&delimiter);
    }

    printf("host : %s\n",host);
    printf("filepath: %s\n", filepath);

}


struct hostent *getHostname(char *host)
{
    struct hostent *h;
    if ((h=gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        exit(1);
    }

        printf("Host name  : %s\n", h->h_name);
        printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

        return h;
}