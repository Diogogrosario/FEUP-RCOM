#pragma once

int verifyBCC();

int readInfo(int fd, unsigned char * appPacket);

int readSET();

void setupReaderConnection(int fd);

int openReader(char * port);

void closeReader(int fd);

int sendReaderDISC(int fd);

int receiverDisconnect(int fd);

int infoStateMachine(unsigned char *buf, int fd, unsigned char * msg);

int receiverReadUA(int status, int fd);

int setStateMachine(char *buf);