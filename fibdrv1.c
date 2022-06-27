#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include "bn.h"


//#include "fibonacci.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000
#define BUFLEN 4096

#define FIBENCODE 0

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ktime_t kt;
static ktime_t kt1;

// static const unsigned int SIZE = 200;
// 4 is not a fibonacci number, so using it as initialized value.
static const uint64_t INIT = 4;



static void fib_helper(unsigned int n, uint64_t f[])
{
    if (n == 0) {
        f[0] = 0;
        f[1] = 1;
        return;
    }

    unsigned int k = 0;
    if (n % 2) {
        k = (n - 1) / 2;
        fib_helper(k, f);
        uint64_t a = f[0];  // F(k) = F((n-1)/2)
        uint64_t b = f[1];  // F(k + 1) = F((n- )/2 + 1)
        uint64_t c =
            a * (2 * b - a);  // F(n-1) = F(2k) = F(k) * [2 * F(k + 1) - F(k)]
        uint64_t d = a * a + b * b;  // F(n) = F(2k + 1) = F(k)^2 + F(k+1)^2
        f[0] = d;                    // F(n)
        f[1] = c + d;                // F(n+1) = F(n-1) + F(n)
    } else {
        k = n / 2;
        fib_helper(k, f);
        uint64_t a = f[0];       // F(k) = F(n/2)
        uint64_t b = f[1];       // F(k + 1) = F(n/2 + 1)
        f[0] = a * (2 * b - a);  // F(n) = F(2k) = F(k) * [2 * F(k + 1) - F(k)]
        f[1] = a * a + b * b;    // F(n + 1) = F(2k + 1) = F(k)^2 + F(k+1)^2
    }
}
static void fib_count(uint64_t n, bn *fib)
{
    if (unlikely(n <= 2)) {
        if (n == 0)
            bn_zero(fib);
        else
            bn_set_u32(fib, 1);
        return;
    }

    bn *a1 = fib; /* Use output param fib as a1 */

    bn_t a0, tmp, a;
    bn_init_u32(a0, 0); /*  a0 = 0 */
    bn_set_u32(a1, 1);  /*  a1 = 1 */
    bn_init(tmp);       /* tmp = 0 */
    bn_init(a);

    /* Start at second-highest bit set. */

    for (uint64_t k = ((uint64_t) 1) << (62 - __builtin_clzll(n)); k; k >>= 1) {
        bn_lshift(a0, 1, a);
        bn_add(a, a1, a);
        bn_sqr(a1, tmp);
        bn_sqr(a0, a0);
        bn_add(a0, tmp, a0);
        bn_mul(a1, a, a1);
        if (k & n) {
            bn_swap(a1, a0);
            bn_add(a0, a1, a1);
        }
    }

    /*
    for (int i = 0; i < n / 2; i++) {
        bn_add(a0, a1, a0);
        bn_add(a0, a1, a1);
    }
    if (!(n & 0x01)) {
        bn_swap(a1, a0);
    }
    */


    /* Now a1 (alias of output parameter fib) = F[n] */

    bn_free(a0);
    bn_free(tmp);
    bn_free(a);
}

static int ByteModify(uint32_t *x, char *buffer)
{  // uint64_t pointer and uint32_t pointer

    /*
     if (x == NULL)
     {
         printk(KERN_INFO "uint32_t x is error. \n");
     }
    */
    if (*x == 0) {
        // printk(KERN_INFO "In 0. \n");
        *buffer = 0x00;
        return 1;
    }

    int leading_zero = __builtin_clz(*x);

    int bit_length = 32 - leading_zero;
    // printk(KERN_INFO "Leading_zero = %d ",leading_zero);

    char *tmp1 = (char *) (void *) (x);
    char *tmp2 = (char *) (void *) (x) + 1;
    char *tmp3 = (char *) (void *) (x) + 2;
    char *tmp4 = (char *) (void *) (x) + 3;


    char c0 = *tmp1 & 0x7f;

    char c0_c = (*tmp1 >> 7) & 0x01;

    char c1 = ((*tmp2 & 0x3f) << 1) | 0x80 | c0_c;
    char c1_c = (*tmp2 >> 6) & 0x03;
    char c2 = ((*tmp3 & 0x1f) << 2) | 0x80 | c1_c;
    char c2_c = (*tmp3 >> 5) & 0x07;
    char c3 = ((*tmp4 & 0x0f) << 3) | 0x80 | c2_c;
    char c4 = ((*tmp4 >> 4) & 0x0f) | 0x80;

    int bytesize = -1;


    if (bit_length <= 7) {
        *buffer = c0;
        bytesize = 1;
    } else if (bit_length <= 14) {
        *buffer++ = c1;
        *buffer++ = c0;
        bytesize = 2;
    } else if (bit_length <= 21) {
        *buffer++ = c2;
        *buffer++ = c1;
        *buffer++ = c0;
        bytesize = 3;
    } else if (bit_length <= 28) {
        *buffer++ = c3;
        *buffer++ = c2;
        *buffer++ = c1;
        *buffer++ = c0;
        bytesize = 4;
    } else if (bit_length <= 32) {
        *buffer++ = c4;
        *buffer++ = c3;
        *buffer++ = c2;
        *buffer++ = c1;
        *buffer++ = c0;
        bytesize = 5;
    }

    return bytesize;
}

static int encoder(apm_digit *x, char *result, int size)
{
    uint32_t *pt = (uint32_t *) (void *) x;

    if (size == 0) {
        *result = 0x00;
        return 1;
    }


    int total_size = 0;

    size = size * 2;

    pt = pt + size - 1;

    do {
        char *buffer = kmalloc((sizeof(char) * 5), GFP_KERNEL);
        /*
        if (buffer == NULL){
            printk(KERN_INFO "allocate buffer error: ");
            }
        */
        // printk(KERN_INFO "before ByteModify  \n",total_size);
        int bytes_size = ByteModify(pt, buffer);

        memcpy((void *) result, buffer, bytes_size);

        result = result + bytes_size;
        total_size += bytes_size;

        pt--;
        kfree(buffer);
    } while (--size > 0);



    return total_size;
}

// static unsigned int fibo_size;



static int fib_sequence(long long n, char *buf, size_t size)
{
    // uint64_t f[2] = { INIT, INIT };
    // fib_helper(n, f);
    // printk(KERN_INFO "sizeof %d %d %d
    // \n",sizeof(uint32_t),sizeof(uint64_t),sizeof(unsigned long long));

    bn_t fib = BN_INITIALIZER;
    unsigned long valid;
    fib_count(n, fib);
    // fibo_size = fib->size;
    int total_size = fib->size;
    if (FIBENCODE) {
        int estimate_size = 10 * (fib->size);
        if (fib->size == 0) {
            estimate_size = 1;
            // printk(KERN_INFO "fib_size = 0 ");
        }
        char *result = kmalloc((sizeof(char) * estimate_size), GFP_KERNEL);
        /*
        if (result == NULL  ){
            printk(KERN_INFO "allocate result error: ");
        }
        */
        total_size = encoder(fib->digits, result, fib->size);
        // printk(KERN_INFO "total bytes is : %d \n",total_size);
        valid = copy_to_user(buf, (const void *) result, total_size);
        if (!valid) {
            printk(KERN_INFO " copy error \n");
        }

        kfree(result);
    } else {
        char *buffer;
        buffer = kmalloc(size * sizeof(char), GFP_KERNEL);
        bn_snprint(fib, 10, buffer, size);

        // kt = ktime_get();
        if (fib->size == 0)
            valid = copy_to_user(buf, buffer, 1);
        else
            valid = copy_to_user(buf, buffer, 24 * (fib->size));

        if (!valid) {
            printk(KERN_INFO " copy error \n");
        }
        // kt = ktime_sub(ktime_get(), kt);
    }
    /*
    valid=copy_to_user(buf,&(fib->size),sizeof(u_int32_t));
    kt1 = ktime_get();
    valid=copy_to_user(buf+sizeof(u_int32_t),fib->digits,((fib->size)*8));
    kt1 = ktime_sub(ktime_get(), kt1);
    */
    // kfree(buffer);
    bn_free(fib);

    // printk(KERN_INFO "Hello, world!\n");
    // printk(KERN_INFO "Hello, world!%lld=%lu valid=%ld \n ",n,size,valid);
    return total_size;

    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    // long long f[k + 2];

    // f[0] = 0;
    // f[1] = 1;
    /*
    struct BigN f[k+2];

    f[0].lower = 0;
    f[0].upper = 0;
    f[1].lower = 1;
    f[1].upper = 0;
    */

    // When n = 2: k = 1 and we want to use F(k+1) to calculate F(2k),
    // However, F(2k) = F(k+1) = F(2) is unknown then.
    /*
    if (n <= 2) {
      return n ? 1 : 0; // F(0) = 0, F(1) = F(2) = 1.
    }

    unsigned int k = n / 2; // k = n/2 if n is even. k = (n-1)/2 if n is odd.
    uint64_t a = fib_sequence(k);
    uint64_t b = fib_sequence(k + 1);

    if (n % 2) { // By F(n) = F(2k+1) = F(k+1)^2 + F(k)^2
      return a * a + b * b;
    }
    // By F(n) = F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
    return a * (2 * b - a);
      */

    /*
     if (n == 0) {
       return 0; // F(0) = 0.
     } else if (n <= 2) {
       return 1; // F(1) = F(2) = 0.
     }

     unsigned int k = 0;
     if (n % 2) { // By F(n) = F(2k+1) = F(k+1)^2 + F(k)^2
       k = (n - 1) / 2;
       return fib_sequence(k) * fib_sequence(k) + fib_sequence(k + 1) *
     fib_sequence(k + 1); } else { // By F(n) = F(2k) = F(k) * [ 2 * F(k+1) –
     F(k) ] k = n / 2; return fib_sequence(k) * (2 * fib_sequence(k + 1) -
     fib_sequence(k));
     }
   */
    // printk(KERN_INFO "count %u %u",k,f[k].lower);

    // return f[k];
}

static int fib_open(struct inode *inode, struct file *file)
{
    // if (!mutex_trylock(&fib_mutex)) {
    //   printk(KERN_ALERT "fibdrv is in use");
    //    return -EBUSY;
    //}
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    // mutex_unlock(&fib_mutex);
    return 0;
}
/*
static long long fib_time_proxy(long long k)
{

    long long result = fib_sequence(k);


    return result;
}
*/
/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    /*
    struct BigN k_fib=fib_sequence(*offset);

    unsigned long temp = copy_to_user(buf,&(k_fib.lower),8);
    unsigned long temp1 = copy_to_user(buf+8,&(k_fib.upper),8);
    printk(KERN_INFO "read: %u ,success number : %u ",k_fib.lower,temp);
    return temp+temp1;
    */

    kt = ktime_get();
    unsigned long result = fib_sequence(size, buf, BUFLEN);


    // copy_to_user(&result,buf,8);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) result;
    // return (ssize_t) fib_sequence(*offset);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    if (size == 3)
        return fibo_size;

    // if (size == 2)              // use size to switch copy_to_user analyze
    // strategy
    //    return  ktime_to_ns(kt1);
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
