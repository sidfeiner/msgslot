//
// Created by student on 04/12/2020.
//

#include "message_slot.h"
// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__ioc
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

static LinkedList *devices[MAX_DEVICES] = {NULL};

ListNode *findChannelId(LinkedList *lst, int channelId) {
    ListNode *node = lst->first;
    while (node != NULL) {
        if (node->channelId == channelId) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

void setMsg(ListNode *node, const char *msg, int msgLength) {
    int i;
    for (i = 0; i < msgLength; ++i) {
        get_user(node->msg[i], &msg[i]);
    }
    node->msgLength = msgLength;
}

ListNode *createNode(int channelId, const char *msg, int msgLength) {
    ListNode *node;
    node = kmalloc(sizeof(ListNode), GFP_KERNEL);
    if (node == NULL) return NULL;
    node->msg = kmalloc(sizeof(char) * MSG_MAX_LENGTH, GFP_KERNEL);
    if (node->msg == NULL) return NULL;
    setMsg(node, msg, msgLength);
    node->channelId = channelId;
    node->next = NULL;
    return node;
}

/**
 * Inserts msg into channelId
 * return 0 if successfull, -1 if some error occurred
 */
int insertMsg(LinkedList *lst, int channelId, const char *msg, int msgLength) {
    ListNode *newNode, *prevNode;
    ListNode *node;
    if (lst->size == 0) {
        newNode = createNode(channelId, msg, msgLength);
        if (newNode == NULL) return -1;
        lst->first = newNode;
        lst->size++;
        return 0;
    } else {
        prevNode = NULL;
        node = lst->first;
        do {
            if (channelId == node->channelId) {
                setMsg(node, msg, msgLength);
                return 0;
            } else if (channelId < node->channelId) {
                newNode = createNode(channelId, msg, msgLength);
                if (newNode == NULL) return -1;
                if (prevNode == NULL) {
                    newNode->next = node;
                    lst->first = newNode;
                } else {
                    newNode->next = node;
                    prevNode->next = newNode;
                }
                lst->size++;
                return 0;
            }
            prevNode = node;
            node = node->next;
        } while (node != NULL);

        newNode = createNode(channelId, msg, msgLength);
        if (newNode == NULL) return -1;
        prevNode->next = newNode;
        lst->size++;
        return 0;
    }
}

void free_node(ListNode *node) {
    kfree(node->msg);
}

void freeLst(LinkedList *lst) {
    ListNode *prev, *cur;
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
    printk("Invoking device_open(%p)\n", file);

    minor = iminor(inode);
    if (devices[minor+1] == NULL) {
        printk("adding minor %d to array at %p\n", minor,devices + minor + 1);
        devices[minor+1] = kmalloc(sizeof(LinkedList), GFP_KERNEL);
        if (devices[minor+1] == NULL) return -1;
        devices[minor+1]->size = 0;
        printk("kmalloc returned pointer to %p\n", devices[minor+1]);
    } else {
        printk("minor already registered\n");
    }

    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release(struct inode *inode,
                          struct file *file) {
    printk("Invoking device_release(%p,%p)\n", inode, file);
    minor = -1;
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset) {
    int i;
    unsigned long channelId;
    LinkedList *lst;
    ListNode *node;
    char *msgPtr, *bufferPtr;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channelId = (unsigned long) file->private_data;
    printk("reading device and channelId: %lu from device pointer: %p\n", channelId, devices[minor+1]);
    lst = devices[minor+1];
    node = findChannelId(lst, channelId);
    if (node != NULL) {
        if (length < node->msgLength || buffer == NULL) {
            return -ENOSPC;
        }
        for (i = 0, msgPtr = node->msg, bufferPtr = buffer; i < node->msgLength; ++i, msgPtr++, bufferPtr++) {
            put_user(*msgPtr, bufferPtr);
            printk("successfully read %c\n", *msgPtr);
        }
        return node->msgLength;
    }
    return -EWOULDBLOCK;
}

//----------------------------------------------------------------
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param) {
    // Switch according to the ioctl called
    if (IOCTL_MSG_SLOT_CHNL == ioctl_command_id && ioctl_param != 0) {
        file->private_data = (void *) ioctl_param;
        // Get the parameter given to- ioctl by the process
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

    unsigned long channelId;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    if (length > BUF_LEN || length <= 0) {
        return -EMSGSIZE;
    }
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channelId = (unsigned long) file->private_data;
    printk("device write to device and channelId: %lu\n", channelId);
    if (buffer == NULL) {
        return -ENOSPC;
    }
    if (insertMsg(devices[minor+1], channelId, buffer, length)< 0) {
        return -1;
    }
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
    // Register driver capabilities. Obtain major num
    printk("registering with major %d and name %s\n", MAJOR_NUM, DEVICE_RANGE_NAME);
    major = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    printk("returned with major %d\n", major);
    // Negative values signify an error
    if (major < 0) {
        printk(KERN_ERR "%s registraion failed for  %d\n",
               DEVICE_RANGE_NAME, major);
        return major;
    }
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
    printk("freeing devices starting from %p up to %p\n", devices, limit);
    for (i=0,tmp=devices;tmp< limit;tmp++, i++) {
        if (*tmp != NULL) {
            printk("deleting\n");
            freeLst(*tmp);
        }
    }
    printk("done freeing, unregistering major %d.\n", major);
    unregister_chrdev(major, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);

module_exit(simple_cleanup);

//========================= END OF FILE =========================
