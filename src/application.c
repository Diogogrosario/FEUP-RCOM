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
    int bytesRead = sendInfo(buffer,length,fd);
    readRR(app.fileDescriptor);
    return bytesRead;
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

int buildControlPacket(char * filename,long filesize,char* pack, char controlField){
    pack[0] = '\0';
    pack[0] = controlField;
    pack[1] = FILESIZE;
    pack[2] = sizeof(long);
    memcpy(pack + 3,&filesize,sizeof(long));
    pack[3+sizeof(long)] = FILENAME;
    pack[4+sizeof(long)] = strlen(filename);
    memcpy(pack+5+sizeof(long),filename,strlen(filename));
    return 5+sizeof(long)+strlen(filename);
}

int main(int argc, char **argv)
{
    if (argc <3)
    {
        printf("Usage : './application writer [path_to_file] or ./application reader [path_to_file]");
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
        char pack[MAX_SIZE];

        FILE *f1 = fopen(argv[2],"r");
        fseek(f1,0,SEEK_END);
        long filesize = ftell(f1);

        int packSize = buildControlPacket(argv[2],filesize,pack,CONTROL_START);
        llwrite(app.fileDescriptor,pack,packSize);

        fseek(f1,0,SEEK_SET);
        
        while(getc(f1) != EOF){
            char packet[MAX_SIZE];
            char buf[MAX_SIZE-4];

            int bytesRead = fread(buf,1,MAX_SIZE-4,f1);
            
            int size = buildDataPacket(buf,packet,bytesRead);
            
            llwrite(app.fileDescriptor,packet,size);
        }
        packSize = buildControlPacket(argv[2],filesize,pack,CONTROL_END);
        llwrite(app.fileDescriptor,pack,packSize);

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
