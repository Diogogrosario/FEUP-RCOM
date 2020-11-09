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

int UAStateMachine(unsigned char *buf, char aField, int *currentState, int*currentIndex)
{
  switch (*currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      *currentState = FLAG_RCV;
      *currentIndex = *currentIndex+1;;
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == aField)
    {
      *currentState = A_RCV;
      *currentIndex = *currentIndex+1;;
      return TRUE;
    }
    else if (buf[0] == FLAG)
      return TRUE;
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;
  case A_RCV:
    if (buf[0] == UA_C)
    {
      *currentIndex = *currentIndex+1;;
      *currentState = C_RCV;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      *currentIndex = 0;
      *currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;
  case C_RCV:
    if (buf[0] == (UA_C ^ aField))
    {
      *currentIndex = *currentIndex+1;;
      *currentState = BCC_OK;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      *currentIndex = 0;
      *currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;
  case BCC_OK:
    if (buf[0] == FLAG)
    {
      *currentIndex = *currentIndex+1;;
      *currentState = DONE;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;

  default:
    break;
  }
  *currentIndex = 0;
  return FALSE;
}

int discStateMachine(int status, unsigned char *buf, int * currentState, int* currentIndex)
{
  switch (*currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      *currentState = FLAG_RCV;
      *(currentIndex) = *currentIndex+1;
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == status)
    {
      *currentState = A_RCV;
      *currentIndex = *currentIndex+1;
      return TRUE;
    }
    else if (buf[0] == FLAG)
      return TRUE;
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;
  case A_RCV:
    if (buf[0] == DISC_C)
    {
      *currentIndex = *currentIndex+1;
      *currentState = C_RCV;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      *currentIndex = 0;
      *currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;
  case C_RCV:
    if (buf[0] == (DISC_C ^ status))
    {
      *currentIndex = *currentIndex+1;
      *currentState = BCC_OK;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      *currentIndex = 0;
      *currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
  case BCC_OK:
    if (buf[0] == FLAG)
    {
      *currentIndex = *currentIndex+1;
      *currentState = DONE;
      return TRUE;
    }
    else
    {
      *currentIndex = 0;
      *currentState = START;
    }
    break;

  default:
    break;
  }
  *currentIndex = 0;
  return FALSE;
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