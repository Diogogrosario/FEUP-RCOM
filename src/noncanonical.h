#pragma once

int verifyBCC();

int readInfo(int fd, unsigned char * appPacket);

int readSET();

void setupReaderConnection(int fd);

int openReader(char * port);

void closeReader(int fd);

int sendReaderDISC(int fd);

int receiverDisconnect(int fd);