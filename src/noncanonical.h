#pragma once

int verifyBCC();

int readInfo(int fd, char * appPacket);

int readSET();

void setupReaderConnection(int fd);

int openReader(char * port);

void closeReader(int fd);