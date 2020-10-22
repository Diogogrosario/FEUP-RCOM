#pragma once

#define TRANSMITTER 0
#define RECEIVER 1

struct applicationLayer
{
    int fileDescriptor; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
};

int llopen(char * port, int status);

