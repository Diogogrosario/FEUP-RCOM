#include "writenoncanonical.h"
#include "noncanonical.h"
#include "application.h"
#include "common.h"

#define CHUNK_SIZE 250

struct applicationLayer app;

int llopen(char * port, int status)
{
    int fd;
    
    if(status == TRANSMITTER){
        fd = openWriter(port);
        setupWriterConnection(fd);
    }
    if(status == RECEIVER){
        fd = openReader(port);
        setupReaderConnection(fd);
    }

    return fd;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage : './application writer or ./application reader!");
        exit(1);
    }
    if (!strcmp(argv[1], "writer"))
    {
        app.status = TRANSMITTER;
        app.fileDescriptor = llopen("/dev/ttyS10", TRANSMITTER);
    }
    else if (!strcmp(argv[1], "receiver"))
    {
        app.status = RECEIVER;
        app.fileDescriptor = llopen("/dev/ttyS11", RECEIVER);
        
    }
    else
    {
        printf("Usage : './application writer or ./application reader!");
        exit(1);
    }

    if (!strcmp(argv[1], "writer"))
    {
        char msg[] = "padoru";
        sendInfo(msg,strlen(msg),app.fileDescriptor);
        readRR(app.fileDescriptor);

        closeWriter(app.fileDescriptor);
    }
    else if (!strcmp(argv[1], "receiver"))
    {
        readInfo(app.fileDescriptor);
        closeReader(app.fileDescriptor);
    }

}