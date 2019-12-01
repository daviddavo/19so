#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/version.h> /* For LINUX_VERSION_CODE */

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0

/* 
 * Prototipos
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "chardev"
#define BUF_LEN 5

struct tty_driver* kbd_driver= NULL;
static int Device_Open = 0;
static int major;
static dev_t start;
struct cdev * chardev; 
static char output[BUF_LEN];
static char * ouput_ptr;

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void)
{
    printk(KERN_INFO "modleds: loading\n");
    printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) )
    return vc_cons[fg_console].d->port.tty->driver;
#else
    return vc_cons[fg_console].d->vc_tty->driver;
#endif
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask)
{
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) )
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
#else
    return (handler->ops->ioctl) (vc_cons[fg_console].d->vc_tty, NULL, KDSETLED, mask);
#endif
}

static int __init modleds_init(void)
{
    int minor;		/* Minor number assigned to the associated character device */
    int ret;
    printk(KERN_DEBUG "In modleds_init()");

    /* Get available (major,minor) range */
    if ((ret=alloc_chrdev_region (&start, 0, 1,DEVICE_NAME))) {
        printk(KERN_INFO "Can't allocate chrdev_region()");
        return ret;
    }

    /* Create associated cdev */
    if ((chardev=cdev_alloc())==NULL) {
        printk(KERN_INFO "cdev_alloc() failed ");
        unregister_chrdev_region(start, 1);
        return -ENOMEM;
    }

    cdev_init(chardev,&fops);

    if ((ret=cdev_add(chardev,start,1))) {
        printk(KERN_INFO "cdev_add() failed ");
        kobject_put(&chardev->kobj);
        unregister_chrdev_region(start, 1);
        return ret;
    }

    major=MAJOR(start);
    minor=MINOR(start);

    return SUCCESS;
}

static void __exit modleds_exit(void)
{
    set_leds(kbd_driver,ALL_LEDS_OFF);
    if(chardev)
        cdev_del(chardev);
    unregister_chrdev(major, DEVICE_NAME);
    unregister_chrdev_region(major, 1);
}

static int device_open(struct inode * inode, struct file * file) {
    if (Device_Open) {
        return -EBUSY;
    }

    Device_Open++;

    try_module_get(THIS_MODULE);

    return SUCCESS;
}

static int device_release(struct inode * inode, struct file * file) {
    Device_Open--;

    module_put(THIS_MODULE);

    return 0;
}

static ssize_t device_read(struct file * filp, char * buffer, size_t length, loff_t * offset) {
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return -EPERM;
}

static ssize_t device_write(struct file * filp, const char * buffer, size_t length, loff_t * offset) {
    char * kbuf;
    unsigned int ledmask = 0;
    int i;

    kbuf = kvmalloc(sizeof(char) * (length+1), GFP_KERNEL);

    if(copy_from_user(kbuf, buffer, length)) {
        return -EINVAL;
    }

    for (i = 0; i < length; i++) {
        if ('1' <= kbuf[i] && kbuf[i] <= 3) {
            ledmask = ledmask | 0b1 << (kbuf[i]-'0');
        }
    }

    set_leds(kbd_driver, ledmask);

    return SUCCESS;
}

module_init(modleds_init);
module_exit(modleds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");
