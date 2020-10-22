#pragma once

int verifyBCC();

int readInfo();

int readSET();

void setupReaderConnection(int fd);

int openReader(char * port);

void closeReader(int fd);