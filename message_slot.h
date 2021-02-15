//
// Created by student on 04/12/2020.
//

#ifndef MSGSLOT_MESSAGE_SLOT_H
#define MSGSLOT_MESSAGE_SLOT_H

#include <linux/ioctl.h>

// The major device number.
// We don't rely on dynamic registration
// any more. We want ioctls to know this
// number at compile time.
#define MAJOR_NUM 240

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0

#endif //MSGSLOT_MESSAGE_SLOT_H