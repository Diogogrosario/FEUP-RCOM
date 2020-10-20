/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define BAUDRATE B38400
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
#define RR_C_0 0x05
#define RR_C_1 0x85
#define REJ_C_0 0x01
#define REJ_C_1 0x81
#define DISC_C 0x0B
#define ESCAPE 0x7D
#define ESCAPEFLAG 0x5E
#define ESCAPEESCAPE 0x5D

volatile int STOP = FALSE;

int currentState = 0;
int Nr = 1;

char msg[255];
int res;
int fd;
int currentIndex = -1;
unsigned char frame[255];
int frameSize = 0;

void updateNr()
{
  Nr = (Nr+1)%2;
}

int sendSupervisionPacket(unsigned char addressField,unsigned char controlByte)
{
  unsigned char sendBuf[255];
  sendBuf[0] = FLAG;
  sendBuf[1] = addressField;
  sendBuf[2] = controlByte;
  sendBuf[3] = addressField ^ controlByte;
  sendBuf[4] = FLAG;
  // printf("Flag: %x          sendbuf[0]: %x\n",FLAG,sendBuf[0]);
  // printf("addField: %x          sendbuf[1]: %x\n",addressField,sendBuf[1]);
  // printf("control: %x          sendbuf[2]: %x\n",controlByte,sendBuf[2]);
  // printf("BCC: %x          sendbuf[3]: %x\n",addressField ^ controlByte,sendBuf[3]);
  // printf("Flag: %x          sendbuf[4]: %x\n",FLAG,sendBuf[4]);

  frame[0] = '\0';
  res = write(fd, sendBuf, 5);
  memcpy(frame,sendBuf,5);
  frameSize=5;

  printf("\nsending packet: ");
  fflush(stdout);
  write(1, sendBuf, 5);
  printf(" with a total size of %d bytes\n", res);
  return 0;
}

int verifyBCC()
{
  // printf("\n\n\n");
  // printf(" message length: %ld\n", currentIndex);
  // printf(" last unsigned char os msg : %c\n", msg[currentIndex - 1]);

  unsigned char bccControl = '\0';
  for (int i = 4; i < currentIndex - 1; i++)
  {
    // printf(" calculting new bcc with : %c\n", msg[i]);
    bccControl ^= msg[i];
  }
  if (msg[currentIndex - 1] == bccControl)
  {
    printf("\nBCC OK accepting data\n");
    return TRUE;
  }
  // printf("\n\n\n");
  // printf("Calculated BCC : %c", bccControl);
  // printf("\n\n\n");
  return FALSE;
}

int infoStateMachine(char *buf)
{

  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      currentIndex = -1;
      currentState = FLAG_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == SENDER_A)
    {
      currentState = A_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (buf[0] == FLAG)
      return TRUE;
    else
    {
      currentState = START;
    }
    break;
  case A_RCV:
    if (Nr == 1 && buf[0] == INFO_C_0)
    {
      currentState = C_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (Nr == 0 && buf[0] == INFO_C_1)
    {
      currentState = C_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentState = START;
    }
    break;
  case C_RCV:
    if (Nr == 1 && buf[0] == (SET_C ^ INFO_C_0))
    {
      currentState = BCC_OK;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (Nr == 0 && buf[0] == (SET_C ^ INFO_C_1))
    {
      currentState = BCC_OK;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentState = START;
    }
  case BCC_OK: //receives info
    if (buf[0] == FLAG)
    {
      verifyBCC();
      msg[currentIndex] = buf[0];
      currentIndex++;
      res = 0;
      currentState = DONE;
      write(1,msg,currentIndex);
      return TRUE;
    }
    else if (buf[0] == ESCAPE)
    {
      res += read(fd, buf, 1);
      if (buf[0] == ESCAPEFLAG)
      {
        msg[currentIndex] = '~';
        currentIndex++;
        res = 0;
      }
      else if (buf[0] == ESCAPEESCAPE)
      {
        msg[currentIndex] = '}';
        currentIndex++;
        res = 0;
      }
      else
      {
        msg[currentIndex] = '}';
        currentIndex++;
        msg[currentIndex] = buf[0];
        currentIndex++;
        res = 0;
      }
      return TRUE;
    }
    else
    {
      msg[currentIndex] = buf[0];
      currentIndex++;
      res = 0;
      return TRUE;
    }
    break;

  default:
    break;
  }

  return FALSE;
}

int setStateMachine(char *buf)
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
    else if (buf[0] == FLAG){
      currentIndex=1;
      return TRUE;
    }
    else
    {
      currentState = START;
    }
    break;
  case A_RCV:
    if (buf[0] == SET_C)
    {
      currentState = C_RCV;
      currentIndex++;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex=1;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
      currentState = START;
    }
    break;
  case C_RCV:
    if (buf[0] == (SET_C ^ SENDER_A))
    {
      currentIndex++;
      currentState = BCC_OK;
      return TRUE;
    }
    else if (buf[0] == FLAG)
    {
      currentIndex=1;
      currentState = FLAG_RCV;
      return TRUE;
    }
    else
    {
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
  currentIndex=0;
  return FALSE;
}

int main(int argc, char **argv)
{

  struct termios oldtio, newtio;
  char buf[255];

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

  fd = open(argv[1], O_RDWR | O_NOCTTY);
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
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 1 char received */

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

  printf("New termios structure set\n");

  //RECEIVE
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, buf, 1); /* returns after 5 chars have been input */
    if (setStateMachine(buf))
    {
      msg[currentIndex] = buf[0];
      res = 0;
      if (currentState == DONE)
      {
        printf("received SET message ");
        fflush(stdout);
        write(1, msg, currentIndex+1);
        printf(" with a total size of %d bytes", currentIndex+1);
        currentIndex = -1;
        currentState = START;
        STOP = TRUE;
        msg[0] = '\0';
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }

  //WRITE BACK
  sendSupervisionPacket(SENDER_A, UA_C);
  STOP = FALSE;

  /* 
    ler os dados
  */
  while (1)
  {
    while (STOP == FALSE)
    {
      /* loop for input */
      buf[0] = '\0';
      res += read(fd, buf, 1); /* returns after 5 chars have been input */

      if (infoStateMachine(buf))
      {
        if (currentState == DONE)
        {
          currentIndex = -1;
          if(Nr==0)
            sendSupervisionPacket(SENDER_A,RR_C_0);
          else if(Nr==1)
            sendSupervisionPacket(SENDER_A,RR_C_1);

          currentState = START;
          STOP = TRUE;
          updateNr();
        }
      }
      else
      {
        msg[0] = '\0';
      }
    }
    STOP = FALSE;
  }

  sleep(1);
  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}
