/*Non-Canonical Input Processing*/

#include "writenoncanonical.h"
#include <sys/types.h>
#include "common.h"
#include "application.h"

static volatile int STOP = FALSE;

struct linkLayer protocol;
struct termios oldtio, newtio;


static int currentState = 0;
static int currentPos = 4;
unsigned char msg[MAX_SIZE*2+7];
unsigned char buf[MAX_SIZE*2+7];
static int currentIndex = 0;
static int activatedAlarm = FALSE;

static int res;

static void atende(int signo) // atende alarme
{
  switch (signo){
    case SIGALRM:
      
      if(protocol.currentTry>=protocol.numTransmissions)
        exit(1);
      protocol.currentTry++;
      activatedAlarm = TRUE;
      break;
  }
}

int writerReadDISC(int status, int fd)
{
  STOP=FALSE;
  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    res += read(fd, recvBuf, 1);
    if(activatedAlarm)
    {
      activatedAlarm = FALSE;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (discStateMachine(status, recvBuf, &currentState,&currentIndex))
    {
      msg[currentIndex] = recvBuf[0];
      res = 0;
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }
  return 0;
}

int openWriter(char * port)
{

    fillProtocol(&protocol, port, 0);
    int fd = open(protocol.port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(protocol.port);
        exit(-1);
    }

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 5;  /* blocking read until 5 chars received */

    /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    printf("Connection established at: %s\n",port);
    return fd;
}

void closeWriter(int fd)
{
   sleep(1);
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
}

int stuffChar(char info, unsigned char *buf)
{
  if (info == FLAG)
  {
    buf[0] = ESCAPE;
    buf[1] = ESCAPEFLAG;
    return TRUE;
  }
  else if (info == ESCAPE)
  {
    buf[0] = ESCAPE;
    buf[1] = ESCAPEESCAPE;
    return TRUE;
  }
  else
  {
    buf[0] = info;
    return FALSE;
  }
}

int sendInfo(unsigned char *info, int size, int fd)
{
  currentPos = 4;

  char sendMessage[size*2+7];
  sendMessage[0] = FLAG;
  sendMessage[1] = SENDER_A;
  if (protocol.sequenceNumber == 1)
    sendMessage[2] = INFO_C_1;
  else if (protocol.sequenceNumber == 0)
    sendMessage[2] = INFO_C_0;

  sendMessage[3] = SENDER_A ^ sendMessage[2];

  unsigned char bcc = '\0';
  int offset = 0;
  for (int i = 0; i < size; i++)
  {

    bcc = bcc ^ info[i];
    unsigned char stuffedBuf[2];

    if (stuffChar(info[i], stuffedBuf))
    {
      sendMessage[4 + i + offset] = stuffedBuf[0];
      offset++;
      sendMessage[4 + i + offset] = stuffedBuf[1];
    }
    else
      sendMessage[4 + i + offset] = stuffedBuf[0];
    currentPos++;
  }

  unsigned char stuffedBCC[2];
  if (stuffChar(bcc, stuffedBCC))
  {
    sendMessage[currentPos + offset] = stuffedBCC[0];
    offset++;
    sendMessage[currentPos + offset] = stuffedBCC[1];
  }
  else
    sendMessage[currentPos + offset] = stuffedBCC[0];
  currentPos++;
  sendMessage[currentPos + offset] = FLAG;
  currentPos++;

  res = write(fd, sendMessage, currentPos + offset + 1);
  protocol.frame[0] = '\0';
  memcpy(protocol.frame, sendMessage, currentPos + offset + 1);
  protocol.frameSize = currentPos + offset + 1;

  alarm(protocol.timeout);
  return res;
}

int writerUaStateMachine(unsigned char *buf)
{
  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      currentState = FLAG_RCV;
      currentIndex++;
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == SENDER_A)
    {
      currentState = A_RCV;
      currentIndex++;
      return TRUE;
    }
    else if (buf[0] == FLAG)
      return TRUE;
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;
  case A_RCV:
    if (buf[0] == UA_C)
    {
      currentIndex++;
      currentState = C_RCV;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex = 0;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;
  case C_RCV:
    if (buf[0] == (UA_C ^ SENDER_A))
    {
      currentIndex++;
      currentState = BCC_OK;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex = 0;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
  case BCC_OK:
    if (buf[0] == FLAG)
    {
      currentIndex++;
      currentState = DONE;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;

  default:
    break;
  }
  currentIndex = 0;
  return FALSE;
}

int transmitterDisconnect(int fd)
{
  sendSupervisionPacket(SENDER_A, DISC_C, &protocol,fd);
  alarm(protocol.timeout);
  protocol.currentTry = 0;
  writerReadDISC(RECEIVER_A,fd);
  sendSupervisionPacket(RECEIVER_A,UA_C, &protocol,fd);
  printf("Connection closed\n");
  return 1;
}

int rrStateMachine(unsigned char *buf, int fd)
{
  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      currentIndex++;
      currentState = FLAG_RCV;
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == SENDER_A)
    {
      currentIndex++;
      currentState = A_RCV;
      return TRUE;
    }
    else if (buf[0] == FLAG)
      return TRUE;
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;
  case A_RCV:
    if (protocol.sequenceNumber == 0 && buf[0] == RR_C_1)
    {
      currentIndex++;
      currentState = C_RCV;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 1 && buf[0] == RR_C_0)
    {
      
      currentIndex++;
      currentState = C_RCV;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 0 && buf[0] == REJ_C_1)
    {
      protocol.currentTry=0;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
      currentState = START;
      currentIndex = 0;
      return FALSE;
    }
    else if (protocol.sequenceNumber == 1 && buf[0] == REJ_C_0)
    {
      
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
      protocol.currentTry=0;
      currentState = START;
      currentIndex = 0;
      return FALSE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex = 0;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;
  case C_RCV:
    if (protocol.sequenceNumber == 0 && buf[0] == (RR_C_1 ^ SENDER_A))
    {
      currentIndex++;
      currentState = BCC_OK;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 1 && buf[0] == (RR_C_0 ^ SENDER_A))
    {
      currentIndex++;
      currentState = BCC_OK;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex = 0;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
  case BCC_OK:
    if (buf[0] == FLAG)
    {
      currentIndex++;
      currentState = DONE;
      return TRUE;
    }
    else
    {
      currentIndex = 0;
      currentState = START;
    }
    break;

  default:
    break;
  }
  currentIndex = 0;
  return FALSE;
}

int writerReadUA(int fd)
{
  STOP=FALSE;

  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    res += read(fd, recvBuf, 1); 
    if(activatedAlarm)
    {
      activatedAlarm = FALSE;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (writerUaStateMachine(recvBuf))
    {
      msg[currentIndex] = recvBuf[0];
      res = 0;
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }
  return 0;
}

int readRR(int fd)
{
  unsigned char recvBuf[5];
  res = 0;
  STOP = FALSE;
  while (STOP == FALSE)
  {
    res += read(fd, recvBuf, 1); 
    if(activatedAlarm)
    {
      activatedAlarm = FALSE;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (rrStateMachine(recvBuf,fd))
    {
      msg[currentIndex] = recvBuf[0];
      res = 0;
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        updateSequenceNumber(&protocol);
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }
  return 0;
}

int setupWriterConnection(int fd)
{
  
  sendSupervisionPacket(SENDER_A, SET_C, &protocol, fd);
  
  struct sigaction psa;
  psa.sa_handler = atende;
  sigemptyset(&psa.sa_mask);
  psa.sa_flags=0;
  sigaction(SIGALRM, &psa, NULL);

  alarm(protocol.timeout);
  writerReadUA(fd);

  return 0;
}
