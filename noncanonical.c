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

volatile int STOP = FALSE;

int currentState = 0;

int res;

int  setStateMachine(char *buf)
{
  
    switch (currentState)
    {
    case 0:
      if (buf[0] == FLAG)
      {
        currentState++;
        return TRUE;
      }
      break;
    case 1:
      if (buf[0] == SENDER_A)
      {
        currentState++;
        return TRUE;
      }
      else if(buf[0]==FLAG)
        return TRUE;
      else
      {
        currentState = 0;
      }
      break;
    case 2:
      if (buf[0] == SET_C)
      {
        currentState++;
        return TRUE;
      }
      else if (buf[0] == FLAG)
      {
        currentState = 1;
        return TRUE;
      }
      else
      {
        currentState = 0;
      }
      break;
    case 3:
      if (buf[0] == (SET_C ^ SENDER_A))
      {
        currentState++;
        return TRUE;
      }
      else if (buf[0] == FLAG)
      {
        currentState = 1;
        return TRUE;
      }
      else
      {
        currentState = 0;
      }
    case 4:
      if (buf[0] == FLAG)
      {
        currentState++;
        return TRUE;
      }
      else
      {
        currentState = 0;
      }
      break;

    default:
      break;
    }

  return FALSE;

}

int main(int argc, char **argv)
{
  int fd, c;
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
  char msg[255];

  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, buf, 1); /* returns after 5 chars have been input */   
    if (setStateMachine(buf))
    {
      strcat(msg,buf);
      write(1, buf, 1);
      res = 0;
      if(currentState==5){
        currentState=0;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
    // printf("received buf (%s) with a total size of %d bytes\n",buf,res);
  }
  //WRITE BACK
  char sendBuf[255];
  sendBuf[0] = FLAG;
  sendBuf[1] = SENDER_A;
  sendBuf[2] = UA_C;
  sendBuf[3] = SENDER_A ^ UA_C;
  sendBuf[4] = FLAG;
  sendBuf[5] = '\0';

  res = write(fd, sendBuf, 6);

  printf("\nanswering writing buf ");
  fflush(stdout);
  write(1, sendBuf, 6);
  printf(" with a total size of %d bytes\n", res);

  /* 
    ler os dados
  */

  // while (STOP == FALSE)
  // {
  //   /* loop for input */
  //   res += read(fd, buf, 1); /* returns after 5 chars have been input */   
  //   if (setStateMachine(buf))
  //   {
  //     strcat(msg,buf);
  //     write(1, buf, 1);
  //     res = 0;
  //     if(currentState==5){
  //       currentState=0;
  //       STOP = TRUE;
  //     }
  //   }
  //   else
  //   {
  //     msg[0] = '\0';
  //   }
  //   // printf("received buf (%s) with a total size of %d bytes\n",buf,res);
  // }
  buf[0] = '\0';
  res = read(fd, buf, 255);
  write(1,buf,res);
  printf("res = %d\n",res);

  sleep(1);
  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}
