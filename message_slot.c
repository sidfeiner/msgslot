//
// Created by student on 04/12/2020.
//

#include "message_slot.h"
// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__ioc
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#define SUCCESS 0
#define MSG_MAX_LENGTH 128
#define MAX_DEVICES ((1u<<20u)+1)

struct chardev_info {
    spinlock_t lock;
};

// used to prevent concurent access into the same device
static int dev_open_flag = 0;

static struct chardev_info device_info;

// device major number
static int major;
static int minor;

typedef struct _ListNode {
    int channelId;
    char *msg;
    int msgLength;
    struct _ListNode *next;
} ListNode;

typedef struct _LinkedList {
    ListNode *first;
    int size;
} LinkedList;

ListNode *findChannelId(LinkedList *lst, int channelId) {
    ListNode *node = lst->first;
    while (node != NULL) {
        if (node->channelId == channelId) {
            printk("found channelId at node %p\n", node);
            return node;
        }
        node = node->next;
    }
    printk("channelId not found\n");
    return NULL;
}

void setMsg(ListNode *node, const char *msg, int msgLength) {
    int i;
    printk("writing message to node at %p", node);
    for (i = 0; i < msgLength; ++i) {
        get_user(node->msg[i], &msg[i]);
        printk("successfully set %c\n", node->msg[i]);
    }
    node->msgLength = msgLength;
}

ListNode* createNode(int channelId, const char *msg, int msgLength) {
    ListNode *node;
    node = kmalloc(sizeof(ListNode), GFP_KERNEL);
    node->channelId = channelId;
    node->msg = kmalloc(sizeof(char) * MSG_MAX_LENGTH, GFP_KERNEL);
    setMsg(node, msg, msgLength);
    node->next=NULL;
    return node;
}

void insertMsg(LinkedList *lst, int channelId, const char *msg, int msgLength) {
    ListNode *newNode, *prevNode;
    ListNode *node;
    printk("inserting message into lst: %p with first: first: %p\n", lst, lst->first);
    if (lst->size == 0) {
        printk("creating node at beginning (first)\n");
        newNode = createNode(channelId, msg, msgLength);
        lst->first = newNode;
        lst->size++;
    } else {
        printk("searching nodes...\n");
        prevNode = NULL;
        node = lst->first;
        do {
            if (channelId == node->channelId) {
                setMsg(node, msg, msgLength);
                return;
            } else if (channelId < node->channelId) {
                printk("creating node in middle\n");
                newNode=createNode(channelId, msg, msgLength);
                if (prevNode == NULL) {
                    newNode->next=node;
                    lst->first = newNode;
                } else {
                    newNode->next = node;
                    prevNode->next = newNode;
                }
                lst->size++;
                return;
            }
            prevNode = node;
            node = node->next;
        } while (node != NULL);

        printk("creating node at end\n");
        newNode=createNode(channelId, msg, msgLength);
        prevNode->next = newNode;
    }

}

static LinkedList *devices[MAX_DEVICES] = {NULL};

void free_node(ListNode *node) {
    printk("freeing node with channelId: %d\n", node->channelId);
    kfree(node->msg);
}

void freeLst(LinkedList *lst) {
    ListNode *prev, *cur;
    printk("freeing list of size %d\n", lst->size);
    prev = lst->first;
    cur = prev->next;
    while (cur != NULL) {
        free_node(prev);
        prev = cur;
        cur = cur->next;
    }
    if (prev != NULL) {
        free_node(prev);
    }
    kfree(lst);
}

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode,
                       struct file *file) {
    unsigned long flags; // for spinlock
    printk("Invoking device_open(%p)\n", file);

    // We don't want to talk to two processes at the same time
    spin_lock_irqsave(&device_info.lock, flags);
    if (1 == dev_open_flag) {
        spin_unlock_irqrestore(&device_info.lock, flags);
        return -EBUSY;
    }

    ++dev_open_flag;
    minor = iminor(inode);
    if (devices[minor+1] == NULL) {
        printk("adding minor %d to array at %p\n", minor,devices + minor + 1);
        devices[minor+1] = kmalloc(sizeof(LinkedList), GFP_KERNEL);
        devices[minor+1]->size = 0;
        printk("kmalloc returned pointer to %p\n", devices[minor+1]);
    } else {
        printk("minor already registered\n");
    }

    spin_unlock_irqrestore(&device_info.lock, flags);
    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release(struct inode *inode,
                          struct file *file) {
    unsigned long flags; // for spinlock
    printk("Invoking device_release(%p,%p)\n", inode, file);

    // ready for our next caller
    spin_lock_irqsave(&device_info.lock, flags);
    --dev_open_flag;
    minor = -1;
    spin_unlock_irqrestore(&device_info.lock, flags);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset) {
    int channelId, i;
    LinkedList *lst;
    ListNode *node;
    char *msgPtr, *bufferPtr;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    channelId = (int)(long) file->private_data;
    printk("reading device and channelId: %d from device pointer: %p\n", channelId, devices[minor+1]);
    lst = devices[minor+1];
    node = findChannelId(lst, channelId);
    if (node != NULL) {
        for (i = 0, msgPtr = node->msg, bufferPtr = buffer; i < node->msgLength; ++i, msgPtr++, bufferPtr++) {
            put_user(*msgPtr, bufferPtr);
            printk("successfully read %c\n", *msgPtr);
        }
        return node->msgLength;
    }
    return -EINVAL;
}

//----------------------------------------------------------------
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param) {
    // Switch according to the ioctl called
    if (IOCTL_MSG_SLOT_CHNL == ioctl_command_id && ioctl_param != 0) {
        file->private_data = (void *)(long) ioctl_param;
        // Get the parameter given to ioctl by the process
        printk("Invoking ioctl: setting channelId "
               "flag to %ld\n", ioctl_param);
        return SUCCESS;
    } else {
        printk("unknown ioctl_command given: %ld\n", ioctl_param);
        return -EINVAL;
    }
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset) {

    int channelId;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    channelId = (int)(long) file->private_data;
    printk("device write to device and channelId: %d\n", channelId);
    insertMsg(devices[minor+1], channelId, buffer, length);
    return length;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
        {
                .owner      = THIS_MODULE, // Required for correct count of module usage. This prevents the module from being removed while used.
                .read           = device_read,
                .write          = device_write,
                .open           = device_open,
                .release        = device_release,
                .unlocked_ioctl          = device_ioctl,
        };

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void) {
    // init dev struct
    memset(&device_info, 0, sizeof(struct chardev_info));
    spin_lock_init(&device_info.lock);

    // Register driver capabilities. Obtain major num
    printk("registering with major %d and name %s\n", MAJOR_NUM, DEVICE_RANGE_NAME);
    major = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    printk("returned with major %d\n", major);
    // Negative values signify an error
    if (major < 0) {
        printk(KERN_ALERT "%s registraion failed for  %d\n",
               DEVICE_RANGE_NAME, major);
        return major;
    }
    printk("first device address: %p,  %p, %p, %p\n", devices, devices[0], devices[1], devices[2]);
    printk("Registeration is successful. YAY "
           "The major device number is %d.\n", major);
    return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void) {
    // Unregister the device
    // Should always succeed
    LinkedList **tmp, **limit;
    int i;
    limit = devices + MAX_DEVICES;
    unregister_chrdev(major, DEVICE_RANGE_NAME);
    printk("freeing devices starting from %p up to %p\n", devices, limit);
    for (i=0,tmp=devices;tmp< limit;tmp++, i++) {
        if (*tmp != NULL) {
            printk("deleting\n");
            freeLst(*tmp);
        }
    }
    printk("done freeing.\n");
}

//---------------------------------------------------------------
module_init(simple_init);

module_exit(simple_cleanup);

//========================= END OF FILE =========================
