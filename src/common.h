#pragma once

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>


#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define SENDER_A 0x03
#define RECEIVER_A 0x01
#define SET_C 0x03
#define UA_C 0x07
#define START 0
#define FLAG_RCV 1
#define A_RCV 2
#define C_RCV 3
#define BCC_OK 4
#define DONE 5
#define INFO_C_0 0x00
#define INFO_C_1 0x40
#define DISC_C 0x0B
#define ESCAPE 0x7D
#define ESCAPEFLAG 0x5E
#define ESCAPEESCAPE 0x5D
#define RR_C_0 0x05
#define RR_C_1 0x85
#define REJ_C_0 0x01
#define REJ_C_1 0x81
#define MAX_SIZE 250


struct linkLayer
{
  char port[20];                 /*Dispositivo /dev/ttySx, x = 0, 1*/
  int baudRate;                  /*Velocidade de transmissão*/
  unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
  unsigned int timeout;          /*Valor do temporizador: 1 s*/
  unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
  int currentTry;                /*Número da tentativa atual em caso de falha*/
  char frame[MAX_SIZE*2+7];          /*Trama*/
  int frameSize;
};

void fillProtocol(struct linkLayer *protocol ,char* port, int Ns);

int sendSupervisionPacket(char addressField, char controlByte,struct linkLayer * protocol, int fd);

void updateSequenceNumber(struct linkLayer *protocol);

int discStateMachine(int status, unsigned char *buf, int * currentState, int* currentIndex);

int UAStateMachine(unsigned char *buf, char aField, int *currentState, int*currentIndex);