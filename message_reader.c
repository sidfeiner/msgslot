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

    if (c != 3) {
        printf("wrong amount of arguments given: %s\n", strerror(1));
        exit(1);
    }

    deviceFile = args[1];
    channelId = strtol(args[2], NULL, 10);
    if (channelId == UINT_MAX && errno == ERANGE) {
        printf("wrong channelId given: %s\n", strerror(2));
        exit(1);
    }

    fd = open(deviceFile, O_RDWR);
    if (fd < 0) {
        printf("could not open file: %s\n", strerror(3));
        exit(1);
    }

    msg = calloc(BUF_LEN, sizeof(char));

    retVal = ioctl(fd, IOCTL_MSG_SLOT_CHNL, (int) channelId);
    retVal = read(fd, msg, BUF_LEN);
    printf("%s\n", msg);


}
