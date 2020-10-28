#include "common.h"

int sendSupervisionPacket(char addressField, char controlByte,struct linkLayer* protocol, int fd)
{
  unsigned char sendBuf[5];
  sendBuf[0] = FLAG;
  sendBuf[1] = addressField;
  sendBuf[2] = controlByte;
  sendBuf[3] = addressField ^ controlByte;
  sendBuf[4] = FLAG;

  write(fd, sendBuf, 5);
  protocol->frame[0] = '\0';
  memcpy(protocol->frame, sendBuf, 5);
  protocol->frameSize = 5;
  return 0;
}



void fillProtocol(struct linkLayer *protocol ,char* port, int Ns){
  strcpy(protocol->port,port);
  protocol->port[strlen(port)] = '\0'; 
  protocol->baudRate = BAUDRATE;
  protocol->sequenceNumber = Ns;
  protocol->timeout = 1;
  protocol->numTransmissions = 3;
  protocol->frame[0] = '\0';
  protocol->frameSize = 0;
  protocol->currentTry = 0;
}

void updateSequenceNumber(struct linkLayer *protocol)
{
  protocol->sequenceNumber = (protocol->sequenceNumber + 1) % 2;
}