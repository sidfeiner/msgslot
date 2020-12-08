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

int main(int c, char **args) {
    char *deviceFile, *msg;
    long channelId;
    int fd, retVal;

    if (c != 4) {
        perror("wrong amount of arguments given");
        exit(1);
    }

    deviceFile = args[1];
    channelId = strtol(args[2], NULL, 10);
    if (channelId == UINT_MAX && errno == ERANGE) {
        perror("wrong channelId given");
        exit(1);
    }
    msg = args[3];

    fd = open(deviceFile, O_RDWR);
    if (fd < 0) {
        perror("could not open file");
        exit(1);
    }

    retVal = ioctl( fd, IOCTL_MSG_SLOT_CHNL, (int)channelId);
    retVal = write( fd, msg, strlen(msg)+1);
    if (retVal != strlen(msg) +1) {
        perror("write did not return correct amount of bytes\n");
    }


}
