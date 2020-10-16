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
#define ESCAPE 0x7D
#define ESCAPEFLAG 0x5E
#define ESCAPEESCAPE 0x5D

volatile int STOP = FALSE;

int currentState = 0;
int Nr = 1;

char msg[255];
int res;
int fd;

int sendUA()
{
  char sendBuf[255];
  sendBuf[0] = FLAG;
  sendBuf[1] = SENDER_A;
  sendBuf[2] = UA_C;
  sendBuf[3] = SENDER_A ^ UA_C;
  sendBuf[4] = FLAG;
  sendBuf[5] = '\0';

  res = write(fd, sendBuf, 6);

  printf("\nanswering with UA message ");
  fflush(stdout);
  write(1, sendBuf, 6);
  printf(" with a total size of %d bytes\n", res);
  return 0;
}

int verifyBCC(){
  printf("\n\n\n");
  printf(" message length: %ld\n",strlen(msg));
  printf(" last char os msg : %c\n",msg[strlen(msg)-1]);

  char bccControl = '\0';
  for(int i = 0;i< strlen(msg)-1;i++){
    printf(" calculting new bcc with : %c\n", msg[i]);
    bccControl ^= msg[i];
  }
  if(msg[strlen(msg)-1] == bccControl){
    printf("\nBCC OK accepting data");
    return TRUE; 
  }
  printf("\n\n\n");
  printf("Calculated BCC : %c",bccControl);
  printf("\n\n\n");
  return FALSE;
}

int infoStateMachine(char *buf)
{

  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      currentState = FLAG_RCV;
      strcat(msg,buf);
      write(1,buf,1);
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == SENDER_A)
    {
      currentState = A_RCV;
      strcat(msg,buf);
      write(1,buf,1);
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
      strcat(msg,buf);
      write(1,buf,1);
      return TRUE;
    }
    else if (Nr == 0 && buf[0] == INFO_C_1)
    {
      currentState = C_RCV;
      strcat(msg,buf);
      write(1,buf,1);
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
      strcat(msg,buf);
      write(1,buf,1);
      return TRUE;
    }
    else if (Nr == 0 && buf[0] == (SET_C ^ INFO_C_1))
    {
      currentState = BCC_OK;
      strcat(msg,buf);
      write(1,buf,1);
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
      strcat(msg,buf);
      write(1,buf,1);
      res = 0;
      currentState = DONE;
      return TRUE;
    }
    else if(buf[0] == ESCAPE)
    {
      res += read(fd, buf, 1);
      if(buf[0] == ESCAPEFLAG){
        strcat(msg,"~");
        write(1,"~", 1);
        res = 0;
      }
      else if(buf[0] == ESCAPEESCAPE){
        strcat(msg, "}");
        write(1, "}", 1);
        res = 0;
      }
      else{
        strcat(msg,"}");
        write(1 ,"}", 1);
        strcat(msg,buf);
        write(1, buf, 1);
        res = 0;
      }
      return TRUE;
    }
    else{
      strcat(msg,buf);
      write(1,buf,1);
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
      return TRUE;
    }
    break;
  case FLAG_RCV:
    if (buf[0] == SENDER_A)
    {
      currentState = A_RCV;
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
    if (buf[0] == SET_C)
    {
      currentState = C_RCV;
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
    if (buf[0] == (SET_C ^ SENDER_A))
    {
      currentState = BCC_OK;
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
  case BCC_OK:
    if (buf[0] == FLAG)
    {
      currentState = DONE;
      return TRUE;
    }
    else
    {
      currentState = START;
    }
    break;

  default:
    break;
  }

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
      strcat(msg, buf);
      res = 0;
      if (currentState == DONE)
      {
        printf("received SET message ");
        fflush(stdout);
        write(1, msg, 6);
        printf(" with a total size of %d bytes", 6);
        currentState = START;
        STOP = TRUE;
        read(fd,buf,1);
        msg[0] = '\0';
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }

  //WRITE BACK
  sendUA();
  STOP = FALSE;

  /* 
    ler os dados
  */
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, buf, 1); /* returns after 5 chars have been input */
    if (infoStateMachine(buf))
    {
      if(currentState==DONE){
        currentState=START;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }

  sleep(1);
  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}
