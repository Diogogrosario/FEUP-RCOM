/*Non-Canonical Input Processing*/

#include <sys/types.h>
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
#define MAX_SIZE 255

volatile int STOP = FALSE;

struct linkLayer
{
  char port[20];                 /*Dispositivo /dev/ttySx, x = 0, 1*/
  int baudRate;                  /*Velocidade de transmissão*/
  unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
  unsigned int timeout;          /*Valor do temporizador: 1 s*/
  unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
  int currentTry;                /*Número da tentativa atual em caso de falha*/
  char frame[MAX_SIZE];          /*Trama*/
  int frameSize;
};

struct linkLayer protocol;

void fillProtocol(char* port, int Ns){
  strcpy(protocol.port,port);
  protocol.port[strlen(port)] = '\0'; 
  protocol.baudRate = BAUDRATE;
  protocol.sequenceNumber = Ns;
  protocol.timeout = 1;
  protocol.numTransmissions = 3;
  protocol.frame[0] = '\0';
  protocol.frameSize = 0;
  protocol.currentTry = 0;
}

int currentState = 0;
int currentPos = 4;
int fd;
unsigned char buf[255];
int notAnswered = 1;
int currentIndex = -1;

int res;

void atende() // atende alarme
{
  if (notAnswered && protocol.currentTry < protocol.numTransmissions)
  {
    write(fd, protocol.frame, protocol.frameSize);
    printf("Resending : ");
    fflush(stdout);
    write(1, protocol.frame, protocol.frameSize + 1);
    printf("\n");
    alarm(protocol.timeout);
    protocol.currentTry++;
  }
  else if (protocol.currentTry >= protocol.numTransmissions)
  {
    alarm(0);
    exit(-1);
  }
}

void updateNs()
{
  protocol.sequenceNumber = (protocol.sequenceNumber + 1) % 2;
}

int sendSupervisionPacket(char addressField, char controlByte)
{
  unsigned char sendBuf[255];
  sendBuf[0] = FLAG;
  sendBuf[1] = addressField;
  sendBuf[2] = controlByte;
  sendBuf[3] = addressField ^ controlByte;
  sendBuf[4] = FLAG;

  res = write(fd, sendBuf, 5);
  protocol.frame[0] = '\0';
  memcpy(protocol.frame, sendBuf, 5);
  protocol.frameSize = 5;

  printf("\nsending packet: ");
  fflush(stdout);
  write(1, sendBuf, 5);
  printf(" with a total size of %d bytes\n", res);
  return 0;
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

int sendInfo(char *info, int size)
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

int rrStateMachine(unsigned char *buf)
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
      atende();
      currentState = START;
      currentIndex = -1;
      return FALSE;
    }
    else if (protocol.sequenceNumber == 1 && buf[0] == REJ_C_0)
    {
      atende();
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

int readUA()
{
  unsigned char recvBuf[255];
  char msg[255];
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, recvBuf, 1); /* returns after 5 chars have been input */
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

int readRR()
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
    if (rrStateMachine(recvBuf))
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
        updateNs();
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

int main(int argc, char **argv)
{ 

  struct termios oldtio, newtio;

  //int i, sum = 0, speed = 0, c;

  if ((argc < 2) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  fillProtocol(argv[1], 0);
  fd = open(protocol.port, O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
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

  //WRITE
  sendSupervisionPacket(SENDER_A, SET_C);

  (void)signal(SIGALRM, atende); // instala  rotina que atende interrupcao

  alarm(protocol.timeout);

  readUA();

  /* 
    Criação de dados.
  */

  char data[255];
  strcpy(data, "Padoru");
  sendInfo(data, strlen(data));
  readRR();
  strcpy(data, "Padoru");
  sendInfo(data, strlen(data));
  readRR();

  sendSupervisionPacket(SENDER_A, DISC_C);
  //readDISC();
  sendSupervisionPacket(RECEIVER_A, UA_C);

  sleep(1);
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
