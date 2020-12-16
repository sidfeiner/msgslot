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
    memcpy(node->msg, msg, msgLength);
    node->msgLength = msgLength;
}

/**
 * Creates node. If it failed allocating memory for the message, it frees the node it created
 * REMARK: If this function return NULL, DO NOT ADD IT TO THE LINKED LIST
 */
ListNode *createNode(int channelId) {
    ListNode *node;
    node = kmalloc(sizeof(ListNode), GFP_KERNEL);
    if (node == NULL) return NULL;
    node->msg = kmalloc(sizeof(char) * MSG_MAX_LENGTH, GFP_KERNEL);  // Allocate message memory
    if (node->msg == NULL) {
        kfree(node);
        return NULL;
    }
    node->channelId = channelId;
    node->next = NULL;
    return node;
}

/**
 * Inserts msg into channelId
 * return NULL if there was a problem allocating memory, ListNode otherwise
 */
ListNode *getOrCreateNode(LinkedList *lst, int channelId) {
    ListNode *newNode, *prevNode, *node;
    if (lst->size == 0) {
        newNode = createNode(channelId);
        if (newNode == NULL) return NULL;
        lst->first = newNode;
        lst->size++;
        return newNode;
    } else {
        prevNode = NULL;
        node = lst->first;
        do {
            if (channelId == node->channelId) {
                return node;
            } else if (channelId < node->channelId) {
                newNode = createNode(channelId);
                if (newNode == NULL) return NULL;
                if (prevNode == NULL) {
                    newNode->next = node;
                    lst->first = newNode;
                } else {
                    newNode->next = node;
                    prevNode->next = newNode;
                }
                lst->size++;
                return newNode;
            }
            prevNode = node;
            node = node->next;
        } while (node != NULL);

        newNode = createNode(channelId);
        if (newNode == NULL) return NULL;
        prevNode->next = newNode;
        lst->size++;
        return newNode;
    }
}

void freeNode(ListNode *node) {
    kfree(node->msg);
    kfree(node);
}

void freeLst(LinkedList *lst) {
    ListNode *prev, *cur;
    if (lst->size!=0) {
        prev = lst->first;
        cur = prev->next;
        while (cur != NULL) {
            freeNode(prev);
            prev = cur;
            cur = cur->next;
        }
        freeNode(prev);
    }
    kfree(lst);
}

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode,
                       struct file *file) {
    printk("Invoking device_open(%p)\n", file);

    minor = iminor(inode);
    if (devices[minor + 1] == NULL) {
        printk("adding minor %d to array\n", minor);
        devices[minor + 1] = kmalloc(sizeof(LinkedList), GFP_KERNEL);
        if (devices[minor + 1] == NULL) return -1;
        devices[minor + 1]->size = 0;
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
    int status;
    unsigned long channelId;
    LinkedList *lst;
    ListNode *node;
    char *tmpBuffer;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channelId = (unsigned long) file->private_data;
    lst = devices[minor + 1];
    node = findChannelId(lst, channelId);
    if (node != NULL) {
        if (length < node->msgLength || buffer == NULL) {  // Ensure we have space to write to
            return -ENOSPC;
        }
        if ((tmpBuffer = kmalloc(sizeof(char) * node->msgLength, GFP_KERNEL)) == NULL) {  // Init temp buffer to allow atomic transaction
            return -ENOSPC;
        }
        memcpy(tmpBuffer, node->msg, node->msgLength); // Copy to temp buffer
        if ((status = copy_to_user(buffer, tmpBuffer, node->msgLength))!=0) { // Write to user buffer
            return -ENOSPC;
        }
        kfree(tmpBuffer);
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
    int status;
    char *tmpBuffer;
    ListNode *node;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    if (length > BUF_LEN || length <= 0) {  // Buffer has wrong size
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

    // Get or create relevant node to store data in
    if ((node = getOrCreateNode(devices[minor + 1], channelId)) == NULL) {
        return -ENOSPC;
    }
    node->msgLength = length;
    if ((tmpBuffer = kmalloc(sizeof(char) * length, GFP_KERNEL)) == NULL) {
        return -ENOSPC;
    }
    if ((status = copy_from_user(tmpBuffer, buffer, length))!=0) {
        printk("copied %d instead of %ld", status, length);
        return -ENOSPC;
    }
    setMsg(node, tmpBuffer, length);
    kfree(tmpBuffer);
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
    int success;
    printk("registering with major %d and name %s\n", MAJOR_NUM, DEVICE_RANGE_NAME);
    success = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    printk("returned with major %d\n", MAJOR_NUM);
    // Negative values signify an error
    if (success != 0) {
        printk(KERN_ERR "%s registraion failed for  %d. received status %d\n",
               DEVICE_RANGE_NAME, MAJOR_NUM, success);
        return MAJOR_NUM;
    }
    printk("Registeration is successful. YAY "
           "The major device number is %d.\n", MAJOR_NUM);
    return 0;
}

//---------------------------------------------------------------
static void __exitsimple_cleanup(void) {
    // Unregister the device
    // Should always succeed
    LinkedList **tmp, **limit;
    limit = devices + MAX_DEVICES;
    printk("freeing memory\n");
    for (tmp = devices + 1; tmp < limit; tmp++) {
        if (*tmp != NULL) {
            freeLst(*tmp);
        }
    }
    printk("done freeing memory\n");
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    printk("successfully unregistered\n");
}

//---------------------------------------------------------------
module_init(simple_init);

module_exit(simple_cleanup);

//========================= END OF FILE =========================
