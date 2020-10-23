#pragma once

#define TRANSMITTER 0
#define RECEIVER 1
#define DATA_C 0x01

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
};

int llopen(char * port, int status);

int llwrite(int fd, char * buffer,int length);

int llread(int fd, char * buffer);
