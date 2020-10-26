#pragma once

#define TRANSMITTER 0
#define RECEIVER 1
#define DATA_C 0x01
#define CONTROL_START 0x02
#define CONTROL_END 0x03
#define FILESIZE 0
#define FILENAME 1
#define READ_C 0
#define READ_NEXT_START 1
#define READ_NEXT_END 2
#define READ_DATA 3

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
};

int llopen(char * port, int status);

int llwrite(int fd, unsigned char * buffer,int length);

int llread(int fd, unsigned char * buffer);
