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

    msg = calloc(BUF_LEN, sizeof(char));

    retVal = ioctl(fd, IOCTL_MSG_SLOT_CHNL, (unsigned long) channelId);
    if (retVal < 0) {
        perror("ioctl failed");
        exitAndClean(fd);
    }
    retVal = read(fd, msg, BUF_LEN);
    if (retVal < 0) {
        perror("read failed");
        exitAndClean(fd);
    }

    clean(fd);
    char *limit = msg + retVal;
    for (char *tmp = msg; tmp < limit; tmp++) {
        printf("%c", *tmp);
    }
    free(msg);
}
