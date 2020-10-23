#include "writenoncanonical.h"
#include "noncanonical.h"
#include "application.h"
#include "common.h"


struct applicationLayer app;
int serialNumber = 0;

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

int llwrite(int fd, char * buffer,int length){
    return sendInfo(buffer,length,fd);
}

int llread(int fd, char * appPacket){
    int size = readInfo(app.fileDescriptor, appPacket);
    return size;
}

int buildDataPacket(char*buf, char* packet, int length){
    packet[0] = '\0';
    packet[0] = DATA_C;
    packet[1] = serialNumber%255;
    packet[2] = length/256;
    packet[3] = length%256;
    int i = 0;
    for(;i<length;i++){
        packet[4+i] = buf[i];
    }
    serialNumber++;
    return 4+i;
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
    else if (!strcmp(argv[1], "reader"))
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
        int charsWritten = -1;
        /*
        FILE *f1 = fopen("pinguim.gif","r");
        fseek(f1,0,SEEK_SET);
        */
        while(charsWritten != 0){
            char packet[MAX_SIZE];
            /*char buf[MAX_SIZE-4];

            int bytesRead = fread(buf,1,MAX_SIZE-4,f1);
            fseek(f1,ftell(f1),ftell(f1)+bytesRead);
            */
           char buf[] ="padoru";
           int bytesRead = strlen("padoru");
           int size = buildDataPacket(buf,packet,bytesRead);
            
            charsWritten = llwrite(app.fileDescriptor,packet,size);
            readRR(app.fileDescriptor);
        }
       

        closeWriter(app.fileDescriptor);
    }
    else if (!strcmp(argv[1], "reader"))
    {
        while(1){
            char appPacket[MAX_SIZE];
            llread(app.fileDescriptor,appPacket);
            //decodeAppPacket();
        }

        closeReader(app.fileDescriptor);
    }

}