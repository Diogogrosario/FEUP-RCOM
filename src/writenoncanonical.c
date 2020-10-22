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
unsigned char buf[255];
static int currentIndex = -1;
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
    newtio.c_cflag = protocol.baudRate | CS8 | CLOCAL | CREAD;
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

int sendInfo(char *info, int size, int fd)
{
  currentPos = 4;

  char sendMessage[255] = "";
  //WRITE
  sendMessage[0] = FLAG;
  sendMessage[1] = SENDER_A;
  sendMessage[2] = INFO_C_1;
  if (protocol.sequenceNumber == 0)
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
  // protocol.frame = sendMessage;
  protocol.frameSize = currentPos + offset + 1;

  write(1, sendMessage, res);
  printf(" with a total size of %d bytes\n", res);
  alarm(protocol.timeout);
  return 1;
}

int uaStateMachine(unsigned char *buf)
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
      currentIndex = -1;
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
      currentIndex = -1;
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
      currentIndex = -1;
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
      currentIndex = -1;
      currentState = START;
    }
    break;

  default:
    break;
  }
  currentIndex = -1;
  return FALSE;
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
      currentIndex = -1;
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
      if(protocol.currentTry>=protocol.numTransmissions)
        exit(1);
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
      protocol.currentTry++;
      currentState = START;
      currentIndex = -1;
      return FALSE;
    }
    else if (protocol.sequenceNumber == 1 && buf[0] == REJ_C_0)
    {
      if(protocol.currentTry>=protocol.numTransmissions)
        exit(1);
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
      protocol.currentTry++;
      currentState = START;
      currentIndex = -1;
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
      currentIndex = -1;
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
      currentIndex = -1;
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
      currentIndex = -1;
      currentState = START;
    }
    break;

  default:
    break;
  }
  currentIndex = -1;
  return FALSE;
}

int readUA(int fd)
{
  unsigned char recvBuf[255];
  char msg[255];
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, recvBuf, 1); /* returns after 5 chars have been input */
    if(activatedAlarm)
    {
      printf("trying again\n");
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (uaStateMachine(recvBuf))
    {
      msg[currentIndex] = recvBuf[0];
      res = 0;
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        printf("received UA message ");
        fflush(stdout);
        write(1, msg, 6);
        printf(" with a total size of %d bytes\n", 6);
        //read(fd,recvBuf,1);
        currentIndex = -1;
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
  unsigned char recvBuf[255];
  char msg[255];
  res = 0;
  STOP = FALSE;
  while (STOP == FALSE)
  {
    //printf("stuck in read\n");
    /* loop for input */
    res += read(fd, recvBuf, 1); /* returns after 5 chars have been input */
    if(activatedAlarm)
    {
      printf("trying again\n");
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
        printf("received RR message ");
        fflush(stdout);
        write(1, msg, currentIndex + 1);
        printf(" with a total size of %d bytes\n", currentIndex + 1);
        updateSequenceNumber(&protocol);
        currentIndex = -1;
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
  readUA(fd);

  return 0;
}

// int main(int argc, char **argv)
// { 


//   /*
//     Open serial port device for reading and writing and not as controlling tty
//     because we don't want to get killed if linenoise sends CTRL-C.
//   */
  

//   //WRITE
  

//   /* 
//     Criação de dados.
//   */

//   char data[255];
//   strcpy(data, "Padoru");
//   sendInfo(data, strlen(data));
//   readRR();
//   strcpy(data, "Padoru");
//   sendInfo(data, strlen(data));
//   readRR();

//   sendSupervisionPacket(SENDER_A, DISC_C, &protocol, fd);
//   //readDISC();
//   sendSupervisionPacket(RECEIVER_A, UA_C, &protocol, fd);

  

//   close(fd);
//   return 0;
// }
