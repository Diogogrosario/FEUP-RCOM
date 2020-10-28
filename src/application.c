#include "writenoncanonical.h"
#include "noncanonical.h"
#include "application.h"
#include "common.h"

struct applicationLayer app;
int serialNumber = 0;
char *fileName;
long fileSize;
char *writeToFile;
int currentFileArrayIndex;
int finished = FALSE;

int llopen(char *port, int status)
{
    int fd;

    if (status == TRANSMITTER)
    {
        fd = openWriter(port);
        setupWriterConnection(fd);
    }
    if (status == RECEIVER)
    {
        fd = openReader(port);
        setupReaderConnection(fd);
    }

    return fd;
}

int llwrite(int fd, unsigned char *buffer, int length)
{
    int bytesRead = sendInfo(buffer, length, fd);
    readRR(app.fileDescriptor);
    return bytesRead;
}

int llread(int fd, unsigned char *appPacket)
{
    int size = readInfo(app.fileDescriptor, appPacket);
    return size;
}

int buildDataPacket(char *buf, unsigned char *packet, int length)
{
    packet[0] = DATA_C;
    packet[1] = serialNumber % 255;
    packet[2] = length / 256;
    packet[3] = length % 256;
    int i = 0;
    for (; i < length; i++)
    {
        packet[4 + i] = buf[i];
    }
    serialNumber++;
    return 4 + i;
}

int buildControlPacket(char *filename, long filesize, unsigned char *pack, char controlField)
{
    pack[0] = controlField;
    pack[1] = FILESIZE;
    pack[2] = sizeof(long);
    memcpy(pack + 3, &filesize, sizeof(long));
    pack[3 + sizeof(long)] = FILENAME;
    pack[4 + sizeof(long)] = strlen(filename);
    memcpy(pack + 5 + sizeof(long), filename, strlen(filename));
    int ret = write(1, pack, 5 + sizeof(long) + strlen(filename));
    
    return ret;
}

int decodeAppPacket(unsigned char *appPacket, int bytesRead)
{
    int state = READ_C;
    static int nextPackSequenceNumber = 0;
    int bytesToSkip = 0;

    while (1 + bytesToSkip < bytesRead)
    {
        switch (state)
        {
        case READ_C:
            if (appPacket[0] == CONTROL_START)
            {
                printf("started reading first control packet\n");
                state = READ_NEXT_START;
            }
            else if (appPacket[0] == CONTROL_END)
            {
                printf("started reading last control packet\n");
                state = READ_NEXT_END;
            }
            else if (appPacket[0] == DATA_C)
            {
                printf("started reading data packet\n");
                state = READ_DATA;
            }
            else
            {
                bytesToSkip = 100000;
            }
            break;
        case READ_NEXT_START:
            if (appPacket[1 + bytesToSkip] == FILENAME)
            {

                printf("started reading first control packet filename\n");
                int size = (int)appPacket[2 + bytesToSkip];
                char filename[size];
                for (int j = 0; j < size; j++)
                {
                    filename[j] = appPacket[3 + bytesToSkip];
                }
                fileName = malloc(sizeof(char) * size);
                strcpy(fileName, filename);
                bytesToSkip += (2 + size);
            }
            else if (appPacket[1 + bytesToSkip] == FILESIZE)
            {
                printf("started reading first control packet filesize\n");
                int size = (int)appPacket[2 + bytesToSkip];
                unsigned char filesize[size];
                for (int i = 0; i < size; i++)
                {
                    filesize[i] = appPacket[3 + bytesToSkip+i];
                    fileSize |= (filesize[i] << 8*i);
                }
                writeToFile = malloc(sizeof(char) * fileSize);
                bytesToSkip += (2 + size);
            }
            break;
        case READ_NEXT_END:
            if (appPacket[1 + bytesToSkip] == FILENAME)
            {
                int size = (int)appPacket[2 + bytesToSkip];
                char filename[size];
                for (int i = 0; i < size; i++)
                {
                    filename[i] = appPacket[3 + bytesToSkip];
                }
                if (!strcmp(filename, fileName))
                {

                    printf("filename match with starting pack\n");
                }
                bytesToSkip += (2 + size);
                if (bytesToSkip + 1 >= bytesRead)
                {
                    finished = TRUE;
                }
            }
            else if (appPacket[1 + bytesToSkip] == FILESIZE)
            {
                int size = (int)appPacket[2 + bytesToSkip];
                char filesize[size];
                for (int i = 0; i < size; i++)
                {
                    filesize[i] = appPacket[3 + bytesToSkip];
                }
                int aux = atoi(filesize);
                if (aux == fileSize)
                {
                    printf("filesize match with starting pack\n");
                }
                bytesToSkip += (2 + size);
                if (bytesToSkip + 1 >= bytesRead)
                {
                    finished = TRUE;
                }
            }
            break;
        case READ_DATA:
            if ((int)appPacket[1] == nextPackSequenceNumber)
            {
                int size = (int)appPacket[2] * 256;
                size += (int)appPacket[3];
                bytesToSkip += 3 + size;
                for (int i = 0; i < size; i++)
                {
                    writeToFile[i + currentFileArrayIndex] = appPacket[4 + i];
                }
                currentFileArrayIndex += size;
                nextPackSequenceNumber = (nextPackSequenceNumber + 1) % 255;
            }
            else
            {
                bytesToSkip = 10000;
            }
            break;
        }
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage : './application writer [path_to_file] or ./application reader [path_to_file]");
        exit(1);
    }
    if (!strcmp(argv[1], "writer"))
    {
        app.status = TRANSMITTER;
        app.fileDescriptor = llopen("/dev/ttyS10", TRANSMITTER);
    }
    else if (!strcmp(argv[1], "reader"))
    {
        app.status = RECEIVER;
        app.fileDescriptor = llopen("/dev/ttyS11", RECEIVER);
    }
    else
    {
        printf("Usage : './application writer or ./application reader!");
        exit(1);
    }

    if (!strcmp(argv[1], "writer"))
    {
        unsigned char pack[MAX_SIZE];

        FILE *f1 = fopen(argv[2], "r");
        fseek(f1, 0, SEEK_END);
        long filesize = ftell(f1);

        int packSize = buildControlPacket(argv[2], filesize, pack, CONTROL_START);
        llwrite(app.fileDescriptor, pack, packSize);

        fseek(f1, 0, SEEK_SET);

        int sizeRemaining = filesize;

        while (sizeRemaining > 0)
        {
            unsigned char packet[MAX_SIZE];
            char buf[MAX_SIZE - 4];

            int bytesRead = fread(buf, 1, MAX_SIZE - 4, f1);

            int size = buildDataPacket(buf, packet, bytesRead);

            sizeRemaining -= bytesRead;

            llwrite(app.fileDescriptor, packet, size);
        }
        packSize = buildControlPacket(argv[2], filesize, pack, CONTROL_END);
        llwrite(app.fileDescriptor, pack, packSize);

        closeWriter(app.fileDescriptor);
    }
    else if (!strcmp(argv[1], "reader"))
    {
        while (!finished)
        {
            unsigned char appPacket[MAX_SIZE];
            int bytesRead = llread(app.fileDescriptor, appPacket);
            decodeAppPacket(appPacket, bytesRead);
        }
        FILE *newFile;
        newFile = fopen("test.gif", "wb");
        fwrite(writeToFile, sizeof(char), fileSize, newFile);
        fclose(newFile);
        closeReader(app.fileDescriptor);
        free(writeToFile);
        free(fileName);
    }
}
