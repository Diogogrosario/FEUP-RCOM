/*Non-Canonical Input Processing*/

#include "noncanonical.h"
#include <sys/types.h>
#include "common.h"

static volatile int STOP = FALSE;

static int currentState = 0;
struct linkLayer protocol;
struct termios oldtio, newtio;


char msg[255];
static int res;
static int currentIndex = -1;

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

int infoStateMachine(char *buf, int fd)
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
    if (protocol.sequenceNumber == 1 && buf[0] == INFO_C_0)
    {
      currentState = C_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 0 && buf[0] == INFO_C_1)
    {
      currentState = C_RCV;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if((protocol.sequenceNumber == 0 && buf[0] == INFO_C_0) || (protocol.sequenceNumber == 1 && buf[0] == INFO_C_1)){
      currentState = START;
      printf("\nDuplicate\n");
      if (protocol.sequenceNumber == 1)
        sendSupervisionPacket(SENDER_A, RR_C_0, &protocol, fd);
      else if (protocol.sequenceNumber == 0)
        sendSupervisionPacket(SENDER_A, RR_C_1, &protocol, fd);
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
    if (protocol.sequenceNumber == 1 && buf[0] == (SET_C ^ INFO_C_0))
    {
      currentState = BCC_OK;
      msg[currentIndex] = buf[0];
      currentIndex++;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 0 && buf[0] == (SET_C ^ INFO_C_1))
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
    else{
      if(protocol.sequenceNumber == 0)
        sendSupervisionPacket(SENDER_A,REJ_C_0, &protocol, fd);
      else if(protocol.sequenceNumber == 1)
        sendSupervisionPacket(SENDER_A,REJ_C_1, &protocol, fd);
      
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
      if (protocol.sequenceNumber == 1 && msg[2] == INFO_C_1)
      {
        msg[0] = '\0';
      }
      else if (protocol.sequenceNumber == 0 && msg[2] == INFO_C_0)
      {
        msg[0] = '\0';
      }
      else
      {
        write(1, msg, currentIndex);
        if (protocol.sequenceNumber == 0)
          sendSupervisionPacket(SENDER_A, RR_C_0, &protocol, fd);
        else if (protocol.sequenceNumber == 1)
          sendSupervisionPacket(SENDER_A, RR_C_1, &protocol, fd);
        updateSequenceNumber(&protocol);
      }
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
    else if (buf[0] == FLAG)
    {
      currentIndex = 1;
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
      currentIndex = 1;
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
      currentIndex = 1;
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
  currentIndex = 0;
  return FALSE;
}

int readInfo(int fd)
{
  char buf[255];
  while (STOP == FALSE)
  {
    /* loop for input */
    buf[0] = '\0';
    res += read(fd, buf, 1); /* returns after 5 chars have been input */

    if (infoStateMachine(buf, fd))
    {
      if (currentState == DONE)
      {
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
  STOP = FALSE;
  return 1;
}

int readSET(int fd)
{
  char buf[255];
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
        write(1, msg, currentIndex + 1);
        printf(" with a total size of %d bytes", currentIndex + 1);
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
  return 0;
}

void closeReader(int fd)
{
    sleep(1);
    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);
}

int openReader(char * port)
{ 
    fillProtocol(&protocol, port, 1);
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

    return fd;
}

void setupReaderConnection(int fd)
{
  //RECEIVE
  readSET(fd);

  //WRITE BACK
  sendSupervisionPacket(SENDER_A, UA_C, &protocol, fd);
}

// int main(int argc, char **argv)
// {
//   STOP = FALSE;

//   /* 
//     ler os dados
//   */
//   while (1)
//   {
//     readInfo();
//   }

//   //readDISC();
//   sendSupervisionPacket(RECEIVER_A, DISC_C, &protocol, fd);
//   //readUA();

//   return 0;
// }
