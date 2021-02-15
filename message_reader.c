//
// Created by sid on 07/12/2020.
//
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include "message_slot.h"

void clean(int fd) {
    close(fd);
}

void exitAndClean(int fd) {
    clean(fd);
    exit(errno);
}

int main(int c, char **args) {
    char *deviceFile, *msg;
    long channelId;
    int fd, retVal;

    if (c != 3) {
        printf("wrong amount of arguments given: %s", strerror(1));
        exit(1);
    }

    deviceFile = args[1];
    channelId = strtol(args[2], NULL, 10);
    if (channelId == UINT_MAX && errno == ERANGE) {
        perror("wrong channelId given: %s");
        exit(1);
    }

    fd = open(deviceFile, O_RDWR);
    if (fd < 0) {
        perror("could not open file: %s");
        exit(1);
    }

    if ((msg = calloc(BUF_LEN, sizeof(char))) == NULL) {
        errno = -1;
        exit(1);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, (unsigned long) channelId) < 0) {
        perror("ioctl failed");
        exitAndClean(fd);
    }
    if ((retVal = read(fd, msg, BUF_LEN)) < 0) {
        perror("read failed");
        exitAndClean(fd);
    }
    clean(fd);
    if ((write(STDOUT_FILENO, msg, retVal) != retVal)) {
        perror("failed writing to STDOUT");
    }
    free(msg);
}
