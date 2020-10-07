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


int fd;
char buf[255];
int notAnswered = 1;
void atende()                   // atende alarme
{
  if(notAnswered){
	  write(fd, buf, strlen(buf)+1);
    printf("resending writing buf (%s) with a total size of %d bytes\n",buf,strlen(buf)+1);
    alarm(3);
  }
}

volatile int STOP = FALSE;

int main(int argc, char **argv)
{
  int c, res;
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

  printf("New termios structure set\n");


  //FLAG FIELD;
  char* flag = malloc(sizeof(char)*8);
  flag = "01111110";

  //A FIELD

  char* sendingA = malloc(sizeof(char)*8); 
  sendingA = "00000011";  //0x03 

  char* receivingA = malloc(sizeof(char)*8); 
  receivingA = "00000001"; //0x01


  // CONTROL FIELD
  char * controlC = malloc(sizeof(char)*8);
  controlC = "00000011"; //0x03

  char * controlC_2 = malloc(sizeof(char)*8);
  controlC_2 = "00000111"; //0x07

  //BCC FIELD

  //XOR BETWEEN A AND C 
  //BCC = a^c decimal converted to bits again 
  char * BCC = malloc(sizeof(char)*8);
  BCC = "00000000"; //in this case 0x03 ^ 0x03 = 0x00;



  //WRITE 
  strcat(buf,flag);
  strcat(buf,sendingA);
  strcat(buf,controlC);
  strcat(buf,BCC);
  strcat(buf,flag);
  
  buf[strlen(buf)] = '\0';
  res = write(fd, buf, strlen(buf)+1);
  printf("writing buf (%s) with a total size of %d bytes\n",buf,res);
  alarm(3); 

  (void) signal(SIGALRM, atende);  // instala  rotina que atende interrupcao

  char recvBuf[255];
  //RECEIVE BACK
  res = 0;
  while (STOP == FALSE) /* loop for input */
  {                                
    res += read(fd, recvBuf, 255); /* returns after 5 chars have been input */
    if(recvBuf[res-1] == '\0')
      STOP = TRUE;
  }
  
  
  printf("received back buf (%s) with a total size of %d bytes\n",recvBuf,res);

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */

  sleep(1);
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
