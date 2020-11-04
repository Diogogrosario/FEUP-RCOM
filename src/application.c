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
    int sizeWritten = 5 + sizeof(long) + strlen(filename);

    return sizeWritten;
}

int decodeAppPacket(unsigned char *appPacket, int bytesRead)
{
    int state = READ_C;
    static int nextPackSequenceNumber = 0;
    int bytesToSkip = 0;
    int filenameOK = FALSE;
    int filesizeOK = FALSE;

    while (1 + bytesToSkip < bytesRead)
    {
        switch (state)
        {
        case READ_C:
            if (appPacket[0] == CONTROL_START)
            {
                state = READ_NEXT_START;
            }
            else if (appPacket[0] == CONTROL_END)
            {
                state = READ_NEXT_END;
            }
            else if (appPacket[0] == DATA_C)
            {
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

                int size = (int)appPacket[2 + bytesToSkip];
                fileName = malloc(sizeof(char) * size);
                for (int j = 0; j < size; j++)
                {
                    fileName[j] = appPacket[3 + bytesToSkip + j];
                }
                bytesToSkip += (2 + size);
            }
            else if (appPacket[1 + bytesToSkip] == FILESIZE)
            {
                int size = (int)appPacket[2 + bytesToSkip];
                unsigned char *filesize = malloc(sizeof(char) * size);
                for (int i = 0; i < size; i++)
                {
                    filesize[i] = appPacket[3 + bytesToSkip + i];
                    fileSize |= (filesize[i] << 8 * i);
                }
                writeToFile = malloc(sizeof(char) * fileSize);
                bytesToSkip += (2 + size);
            }
            break;
        case READ_NEXT_END:
            if (appPacket[1 + bytesToSkip] == FILENAME)
            {
                int size = (int)appPacket[2 + bytesToSkip];
                char *filename = malloc(sizeof(char) * size);
                for (int i = 0; i < size; i++)
                {
                    filename[i] = appPacket[3 + bytesToSkip + i];
                }
                if (!strcmp(filename, fileName))
                {
                    filenameOK = TRUE;
                }
                free(filename);
                bytesToSkip += (2 + size);
                if (bytesToSkip + 1 >= bytesRead)
                {
                    finished = TRUE;
                }
            }
            else if (appPacket[1 + bytesToSkip] == FILESIZE)
            {
                int size = (int)appPacket[2 + bytesToSkip];
                unsigned char *filesize = malloc(sizeof(char) * size);
                long aux = 0;
                for (int i = 0; i < size; i++)
                {
                    filesize[i] = appPacket[3 + bytesToSkip + i];
                    aux |= (filesize[i] << 8 * i);
                }
                if (aux == fileSize)
                {
                    filesizeOK = TRUE;
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

    if (filesizeOK && filenameOK)
    {
        printf("Control Packets matched, file received\n");
    }

    return TRUE;
}

int llclose(int fd)
{
    if (app.status == TRANSMITTER)
    {
        return transmitterDisconnect(fd);
    }
    if (app.status == RECEIVER)
    {
        return receiverDisconnect(fd);
    }
    return -1;
}

int main(int argc, char **argv)
{
    FILE *f1;
    if (!strcmp(argv[1], "writer"))
    {
        if (argc < 3)
        {
            printf("Usage : ./application writer [path_to_file]");
            exit(1);
        }
        f1 = fopen(argv[2], "r");
        if(f1 == NULL){
            printf("File does not exist\n");
            exit(1);
        }
        app.status = TRANSMITTER;
        app.fileDescriptor = llopen("/dev/ttyS0", TRANSMITTER);
    }
    else if (!strcmp(argv[1], "reader"))
    {
        if (argc < 2)
        {
            printf("Usage : ./application reader");
            exit(1);
        }
        app.status = RECEIVER;
        app.fileDescriptor = llopen("/dev/ttyS0", RECEIVER);
    }
    else
    {
        printf("Usage : ./application writer [path_to_file] or./application reader");
        exit(1);
    }

    if (!strcmp(argv[1], "writer"))
    {
        unsigned char pack[MAX_SIZE];

        
        fseek(f1, 0, SEEK_END);
        long filesize = ftell(f1);

        int packSize = buildControlPacket(argv[2], filesize, pack, CONTROL_START);
        printf("Started sending file\n");
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
        printf("Finished sending file\n");

        llclose(app.fileDescriptor);
        closeWriter(app.fileDescriptor);
    }
    else if (!strcmp(argv[1], "reader"))
    {
        printf("Started receiving file\n");
        while (!finished)
        {
            unsigned char appPacket[MAX_SIZE];
            int bytesRead = llread(app.fileDescriptor, appPacket);
            decodeAppPacket(appPacket, bytesRead);
        }
        FILE *newFile;
        char path[256] = "../";
        strcat(path, fileName);
        newFile = fopen(path, "wb");
        fwrite(writeToFile, sizeof(char), fileSize, newFile);
        fclose(newFile);
        printf("File saved in %s\n",path);
        llclose(app.fileDescriptor);
        closeReader(app.fileDescriptor);
        free(writeToFile);
        free(fileName);
    }
}
