#pragma once

int sendInfo(unsigned char *info, int size, int fd);

int stuffChar(char info, unsigned char *buf);

int setupWriterConnection(int fd);

int openWriter(char * port);

void closeWriter(int fd);

int readRR(int fd);

int readUA(int fd);

int transmitterDisconnect(int fd);