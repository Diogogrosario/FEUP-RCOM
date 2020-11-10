/*Non-Canonical Input Processing*/

#include "noncanonical.h"
#include <sys/types.h>
#include "common.h"

struct linkLayer protocol;
struct termios oldtio, newtio;

static int currentState = 0;
static int currentIndex = 0;

static volatile int STOP = FALSE;
int activatedAlarm = FALSE;

static void atende(int signo) // atende alarme
{
  printf("Alarm triggered, resending!\n");
  switch (signo)
  {
  case SIGALRM:

    if (protocol.currentTry >= protocol.numTransmissions)
      exit(1);
    protocol.currentTry++;
    activatedAlarm = TRUE;
    break;
  }
}

int infoStateMachine(unsigned char *buf, int fd, unsigned char * msg)
{
  static unsigned char bccControl;
  static char C;
  switch (currentState)
  {
  case START:
    if (buf[0] == FLAG)
    {
      bccControl = '\0';
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
      currentState = START;
    }
    break;
  case BCC_OK: //receives info
    if (buf[0] == FLAG)
    {
      bccControl = bccControl ^ msg[currentIndex - 1]; // must remove actual bcc from calculated bcc
      if (msg[currentIndex - 1] == bccControl)
      {
        bccControl = '\0';
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
          usleep(100000);
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
        if (protocol.sequenceNumber == 0)
          sendSupervisionPacket(SENDER_A, REJ_C_0, &protocol, fd);
        else if (protocol.sequenceNumber == 1)
          sendSupervisionPacket(SENDER_A, REJ_C_1, &protocol, fd);

        currentState = START;
      }
    }
    else if (buf[0] == ESCAPE)
    {
      read(fd, buf, 1);
      if (buf[0] == ESCAPEFLAG)
      {
        msg[currentIndex] = '~';
        bccControl ^= msg[currentIndex];
        currentIndex++;
      }
      else if (buf[0] == ESCAPEESCAPE)
      {
        msg[currentIndex] = '}';
        bccControl ^= msg[currentIndex];
        currentIndex++;
      }
      else
      {
        msg[currentIndex] = '}';
        bccControl ^= msg[currentIndex];
        currentIndex++;
        msg[currentIndex] = buf[0];
        bccControl ^= msg[currentIndex];
        currentIndex++;
      }
      return TRUE;
    }
    else
    {
      msg[currentIndex] = buf[0];
      bccControl ^= msg[currentIndex];
      currentIndex++;
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
  STOP = FALSE;
  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    read(fd, recvBuf, 1);
    if (activatedAlarm)
    {
      activatedAlarm = FALSE;
      write(fd, protocol.frame, protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (discStateMachine(status, recvBuf, &currentState, &currentIndex))
    {
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
    }
  }
  return 0;
}

int receiverReadUA(int status, int fd)
{
  STOP = FALSE;

  unsigned char recvBuf[5];
  while (STOP == FALSE)
  {
    read(fd, recvBuf, 1);
    if (activatedAlarm)
    {
      activatedAlarm = FALSE;
      write(fd, protocol.frame, protocol.frameSize);
      alarm(protocol.timeout);
    }
    if (UAStateMachine(recvBuf,status,&currentState,&currentIndex))
    {
      if (currentState == DONE)
      {
        alarm(0);
        protocol.currentTry = 0;
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
    }
  }
  return 0;
}

int receiverDisconnect(int fd)
{
  receiverReadDISC(SENDER_A, fd);
  sendSupervisionPacket(RECEIVER_A, DISC_C, &protocol, fd);
  alarm(protocol.timeout);
  protocol.currentTry = 0;
  receiverReadUA(RECEIVER_A, fd);
  printf("Connection closed\n");
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
    break;
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

int readInfo(int fd, unsigned char *appPacket)
{
  
  STOP = FALSE;
  unsigned char msg[MAX_SIZE];
  unsigned char buf[2];
  while (STOP == FALSE)
  {
    buf[0] = '\0';
    read(fd, buf, 1);

    if (infoStateMachine(buf, fd,msg))
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
  for (int i = 0; i < currentIndex - 1; i++)
  {
    appPacket[i] = msg[i];
  }

  ret = currentIndex - 1;
  currentIndex = 0;
  return ret;
}

int readSET(int fd)
{
  STOP = FALSE;

  char buf[5];
  while (STOP == FALSE)
  {
    read(fd, buf, 1);

    if (setStateMachine(buf))
    {
      if (currentState == DONE)
      {
        currentIndex = 0;
        currentState = START;
        STOP = TRUE;
      }
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
  printf("Connection established at: %s\n", port);
  return fd;
}

void setupReaderConnection(int fd)
{
  readSET(fd);

  struct sigaction psa;
  psa.sa_handler = atende;
  sigemptyset(&psa.sa_mask);
  psa.sa_flags = 0;
  sigaction(SIGALRM, &psa, NULL);

  sendSupervisionPacket(SENDER_A, UA_C, &protocol, fd);
}
