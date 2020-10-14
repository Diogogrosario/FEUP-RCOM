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

int currentPos = 4;
int fd;
char buf[255];
int notAnswered = 1;

int res;

int sendSET(){
  buf[0] = FLAG;
  buf[1] = SENDER_A;
  buf[2] = SET_C; 
  buf[3] = SENDER_A ^ SET_C;
  buf[currentPos] = FLAG; 

  res = write(fd, buf, currentPos+2);

  printf("sending SET message ");
  fflush(stdout);
  write(1, buf, currentPos+2);
  printf(" with a total size of %d bytes\n", res);
  return 0;
}

void atende() // atende alarme
{
  if (notAnswered)
  {
    sendSET();
    alarm(3);
  }
}

volatile int STOP = FALSE;

int main(int argc, char **argv)
{
  int c;
  struct termios oldtio, newtio;

  int i, sum = 0, speed = 0;

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
  sendSET();

  (void)signal(SIGALRM, atende); // instala  rotina que atende interrupcao

  alarm(3);

  char recvBuf[255];
  //RECEIVE BACK
  while (STOP == FALSE) /* loop for input */
  {
    res = 0;
    res += read(fd, recvBuf, 255); /* returns after 5 chars have been input */
    if (recvBuf[res - 1] == '\0')
    {
      alarm(0);
      STOP = TRUE;
    }
  }

  printf("received UA message ");
  fflush(stdout);
  write(1, recvBuf, 6);
  printf(" with a total size of %d bytes\n", 6);

  /* 
    Criação de dados.
  */
  buf[0]='\0';

  currentPos = 4;
  //WRITE
  buf[0] = FLAG;
  buf[1] = SENDER_A;
  buf[2] = SET_C; 
  buf[3] = SENDER_A ^ SET_C;
  //INSERIR DADOS AQUI
  //vou tentar escrever padoru
  buf[4] = 'P';
  currentPos++;
  buf[5] = 'a';
  currentPos++;
  buf[6] = 'd';
  currentPos++;
  buf[7] = 'o';
  currentPos++;
  buf[8] = 'r';
  currentPos++;
  buf[9] = 'u';
  currentPos++;
  buf[10] = 'P' ^ 'a' ^ 'd' ^ 'o' ^ 'r' ^ 'u';
  currentPos++;
  buf[currentPos] = FLAG;

  res = write(fd, buf, currentPos+2);
  write(1,buf,res+1);
  printf(" with a total size of %d bytes\n", res);

  sleep(1);
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
