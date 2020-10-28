/*Non-Canonical Input Processing*/

#include "noncanonical.h"
#include <sys/types.h>
#include "common.h"

static volatile int STOP = FALSE;

static int currentState = 0;
struct linkLayer protocol;
struct termios oldtio, newtio;
int activatedAlarm = FALSE;

unsigned char msg[MAX_SIZE*2+7];
static int res;
static int currentIndex = 0;

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

int receiverDiscStateMachine(int status, unsigned char *buf)
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
    if (buf[0] == status)
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
    if (buf[0] == DISC_C)
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
    if (buf[0] == (DISC_C ^ status))
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

int verifyBCC()
{
  unsigned char bccControl = '\0';
  for (int i = 0; i < currentIndex - 1; i++)
  {
    bccControl ^= msg[i];
  }
  if (msg[currentIndex - 1] == bccControl)
  {
    printf("\nBCC OK accepting data\n");
    return TRUE;
  }

  return FALSE;
}

int infoStateMachine(unsigned char *buf, int fd)
{
  
  static char C;
  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      C = '\0';
      currentIndex = 0;
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
    if (protocol.sequenceNumber == 1 && buf[0] == INFO_C_0)
    {
      C = INFO_C_0;
      currentState = C_RCV;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 0 && buf[0] == INFO_C_1)
    {
      C = INFO_C_1;
      currentState = C_RCV;
      return TRUE;
    }
    else if ((protocol.sequenceNumber == 0 && buf[0] == INFO_C_0) || (protocol.sequenceNumber == 1 && buf[0] == INFO_C_1))
    {
      C = '\0';
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
    if (protocol.sequenceNumber == 1 && buf[0] == (SENDER_A ^ INFO_C_0))
    {
      currentState = BCC_OK;
      return TRUE;
    }
    else if (protocol.sequenceNumber == 0 && buf[0] == (SENDER_A ^ INFO_C_1))
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
      if (protocol.sequenceNumber == 0)
        sendSupervisionPacket(SENDER_A, REJ_C_0, &protocol, fd);
      else if (protocol.sequenceNumber == 1)
        sendSupervisionPacket(SENDER_A, REJ_C_1, &protocol, fd);

      currentState = START;
    }
  case BCC_OK: //receives info
    if (buf[0] == FLAG)
    {
      if (verifyBCC())
      {
        res = 0;
        currentState = DONE;
        if (protocol.sequenceNumber == 1 && C == INFO_C_1)
        {
          msg[0] = '\0';
        }
        else if (protocol.sequenceNumber == 0 && C == INFO_C_0)
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
      else
      {
        printf("\nRejecting\n");
        
        if (protocol.sequenceNumber == 0)
          sendSupervisionPacket(SENDER_A, REJ_C_0, &protocol, fd);
        else if (protocol.sequenceNumber == 1)
          sendSupervisionPacket(SENDER_A, REJ_C_1, &protocol, fd);

        currentState = START;
      }
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

int receiverReadDISC(int status, int fd)
{
  STOP=FALSE;
  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, recvBuf, 1); /* returns after 5 chars have been input */
    if(activatedAlarm)
    {
      printf("trying again\n");
      activatedAlarm = FALSE;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (receiverDiscStateMachine(status, recvBuf))
    {
      msg[currentIndex] = recvBuf[0];
      res = 0;
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        printf("received DISC message ");
        fflush(stdout);
        write(1, msg, 6);
        printf(" with a total size of %d bytes\n", 6);
        //read(fd,recvBuf,1);
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

int receiverUaStateMachine(int status,unsigned char *buf)
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
    if (buf[0] == status)
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
    if (buf[0] == (UA_C ^ status))
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

int receiverReadUA(int status, int fd)
{
  STOP=FALSE;

  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    /* loop for input */
    res += read(fd, recvBuf, 1); /* returns after 5 chars have been input */
    if(activatedAlarm)
    {
      printf("trying again\n");
      activatedAlarm = FALSE;
      write(fd,protocol.frame,protocol.frameSize);
      alarm(protocol.timeout); 
    }
    if (receiverUaStateMachine(status,recvBuf))
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

int receiverDisconnect(int fd)
{
  receiverReadDISC(SENDER_A, fd);
  sendSupervisionPacket(RECEIVER_A, DISC_C, &protocol, fd);
  alarm(3);
  receiverReadUA(RECEIVER_A, fd);

  return 1;
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

int readInfo(int fd, unsigned char * appPacket)
{
  STOP = FALSE;
  unsigned char buf[MAX_SIZE*2+7];
  while (STOP == FALSE)
  {
    /* loop for input */
    buf[0] = '\0';
    res += read(fd, buf, 1); /* returns after 5 chars have been input */

    if (infoStateMachine(buf, fd))
    {
      if (currentState == DONE)
      {
        currentState = START;
        STOP = TRUE;
      }
    }
    else
    {
      msg[0] = '\0';
    }
  }
  int ret = 0;
  for(int i = 0;i<currentIndex-1;i++){
    appPacket[i] = msg[i];
  }
  
  ret = currentIndex-1;
  currentIndex = 0;
  return ret;
}

int readSET(int fd)
{
  STOP = FALSE;

  char buf[5];
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
        currentIndex = 0;
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

int openReader(char *port)
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

  //ALARM
  struct sigaction psa;
  psa.sa_handler = atende;
  sigemptyset(&psa.sa_mask);
  psa.sa_flags=0;
  sigaction(SIGALRM, &psa, NULL);

  //WRITE BACK
  sendSupervisionPacket(SENDER_A, UA_C, &protocol, fd);
}