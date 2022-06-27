#include <fcntl.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

//#include "bn.h"

#define FIB_DEV "/dev/fibonacci"
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#define digit_div(n1, n0, d, q, r)                                 \
    do {                                                           \
        uint128_t __d = (d);                                       \
        uint128_t __n = ((uint128_t)(n1) << 64) | (uint128_t)(n0); \
        (q) = (unsigned long) (__n / __d);                         \
        (r) = (unsigned long) (__n % __d);                         \
    } while (0)



/*      UINT64_MAX 18446744073709551615ULL */
#define P10_UINT64 10000000000000000000ULL /* 19 zeroes */
#define E10_UINT64 19

#define STRINGIZER(x) #x
#define TO_STRING(x) STRINGIZER(x)

#define KOBJ "/sys/kernel/kobj_ref/kt_ns"

#define BUFLEN 4096


#define FIBDECODE 0
#define NUM_THREADS 5

typedef unsigned __int128 uint128_t;

/*
long long get_ktime()
{
    int kobj = open(KOBJ, O_RDONLY);

    if (!kobj)
        return -1;

    char buf[32];
    int len = pread(kobj, buf, 31, 0);
    close(kobj);

    if (len < 0)
        return -2;

    buf[len - 1] = '\0';

    return atol(buf);
}
*/
static float size_estimate = 2.40823997f;

static inline long long get_nanotime()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

unsigned int string_estimate(unsigned int size)
{  // radix-2 to  radix 10


    int apm_digit_size = 8;
    return (unsigned int) (size_estimate * size * apm_digit_size) + 2;
}
static const char radix_chars[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int max_power = 19;
static unsigned long max_radix = 0x8AC7230489E80000;

unsigned long ddivi(unsigned long *u, unsigned int size, unsigned long v)
{
    // ASSERT(u != NULL);
    // ASSERT(v != 0);

    if (v == 1)
        return 0;

    while ((size) && !(u)[(size) -1])
        --(size);
    if (!size)
        return 0;
    unsigned long s1 = 0;
    u += size;

    do {
        unsigned long s0 = *--u;
        unsigned long q, r;
        if (s1 == 0) {
            q = s0 / v;
            r = s0 % v;
        } else {
            // uint128_t _d=max_radix;
            // uint128_t _n = ((uint128_t)(s1) << 64) | (uint128_t)(s0) ;
            // q=(unsigned long)(_n / _d);
            // r=(unsigned long)(_n % _d);
            digit_div(s1, s0, v, q, r);
        }
        *u = q;
        s1 = r;

    } while (--size);
    return s1;
}



char *get_str(unsigned long *buf, unsigned int size, char *str)
{
    while ((size) && !(buf)[(size) -1])
        --(size);

    if (size == 0 || (size == 1 && buf[0] < 10)) {
        if (!str)
            // printf("error");
            str = (char *) malloc(2);

        str[0] = size ? radix_chars[buf[0]] : '0';
        str[1] = '\0';

        return str;
    }
    if (!str)
        // printf("error");
        str = (char *) malloc(string_estimate(size) + 1);

    char *outp = str;

    unsigned long *tmp =
        (unsigned long *) memcpy(malloc(size * 8), buf, size * 8);
    // unsigned long *tmp = buf;
    unsigned int tsize = size;


    do {
        unsigned long r = ddivi(tmp, tsize, max_radix);
        tsize -= (tmp[tsize - 1] == 0);

        unsigned int i = 0;
        do {
            unsigned long rq = r / 10;
            unsigned long rr = r % 10;
            *outp++ = radix_chars[rr];
            r = rq;
            if (tsize == 0 && r == 0)
                break;



        } while (++i < max_power);
        // ASSERT(r == 0);
    } while (tsize != 0);

    free(tmp);

    char *f = outp - 1;

    while (*f == '0')
        --f;
    f[1] = '\0';

    for (char *s = str; s < f; ++s, --f)
        SWAP(*s, *f);


    return str;
}



char *Decoder(void *buf, unsigned int size)
{
    const unsigned int string_size = string_estimate(size) + 1;
    // printf("string_size should be : %u \n",string_size);
    char *str;
    str = (char *) malloc(string_size);
    char *p = get_str((unsigned long *) buf, size, str);


    return p;
}
int VarDecoder(char *x, unsigned long long *buffer, int size)
{
    // size = number of *x in x

    int fib_size = 0;

    unsigned long temp = 0;
    unsigned long temp_msb = 0;
    int mergelong = 0;

    unsigned long long *temp_pt = buffer;
    do {
        temp = temp << 7;
        temp = temp | (*x & 0x7f);
        if (!(*x & 0x80))  // x is  the last byte.
        {
            mergelong++;
            if (mergelong == 2) {
                *buffer++ = (((unsigned long long) (temp_msb)) << 32) |
                            (unsigned long long) temp;
                printf(" buffer is:%llu \n", *(buffer - 1));
                fib_size++;
                mergelong = 0;
                temp = 0;
                temp_msb = 0;
            } else {
                temp_msb = temp;
                temp = 0;
            }
        }

        x++;
    } while (--size > 0);

    buffer = buffer - 1;

    for (; temp_pt < buffer; ++temp_pt, --buffer) {
        SWAP(*temp_pt, *buffer);
    }

    return fib_size;
}

typedef struct thread_ID thid;
struct thread_ID {
    int PID;
    int count;
};


void *Fib_count(void *threadnum)
{
    thid *b;
    b = (thid *) threadnum;

    int fd = open(FIB_DEV, O_RDWR);
    int offset = b->count;

    int id = b->PID;


    char write_buf[] = "testing writing";

    for (int i = 0; i <= offset; i++) {
        char buf[4096];
        // lseek(fd, i, SEEK_SET);
        long long start = get_nanotime();


        read(fd, buf, i);
        // printf("Encode size = %d .\n",sz);

        long long utime = get_nanotime() - start;

        long long ktime = write(fd, write_buf, 1);

        printf("Reading from pid : %d " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               id, i, buf);
    }
}



int main()
{
    uint128_t a;
    long long sz;



    char write_buf[] = "testing writing";
    int offset = 10; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    FILE *data = fopen("data.txt", "w");
    FILE *cost = fopen("cost.txt", "w");
    printf("Open character device");
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    pthread_t threads[NUM_THREADS];

    // int rc;


    if (FIBDECODE) {
        for (int i = 0; i <= offset; i++) {
            char buf[4096];
            // lseek(fd, i, SEEK_SET);
            long long start = get_nanotime();


            sz = read(fd, buf, i);
            printf("Encode size = %lld .\n", sz);

            long long utime = get_nanotime() - start;

            long long ktime = write(fd, write_buf, 1);


            unsigned long long *dec_result =
                malloc(sizeof(unsigned long long) * sz);
            int fib_size = VarDecoder(buf, dec_result, sz);

            // int *x;
            // x=(int *)(void *)dec_result;
            // printf("x=%u \n",*x);

            char *a = Decoder((void *) dec_result, fib_size);

            long long decodeTime = get_nanotime() - ktime;

            // char *a = Decoder((void *)(buf+sizeof(unsigned int)),*x);
            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%s.\n",
                   i, a);

            int char_size = 0;
            while (*a != '\0') {
                char_size = char_size + 1;
                a = a + 1;
            }

            fprintf(data, "%d %lld %lld %lld %lld\n", i, ktime, utime,
                    utime - ktime, decodeTime);
            free(dec_result);
            // long long ktime1 = write(fd, write_buf, 2);
            fprintf(cost, "%d %lld %d %d %d \n", i, sz, fib_size * 8, char_size,
                    char_size / 2);
        }
    } else {
        struct thread_ID a[NUM_THREADS];

        for (int i = 0; i < NUM_THREADS; i++) {
            // struct thread_ID a;
            a[i].count = offset;
            a[i].PID = i;

            pthread_create(&threads[i], NULL, Fib_count, (void *) &a[i]);
        }


        /*
        for (int i = 0; i <= offset; i++) {
            char buf[4096];
            //lseek(fd, i, SEEK_SET);
            long long start = get_nanotime();


            sz = read(fd, buf, i);
            printf("Encode size = %d .\n",sz);

            long long utime = get_nanotime() - start;

            long long ktime = write(fd, write_buf, 1);

            printf("Reading from " FIB_DEV
                " at offset %d, returned the sequence "
                "%s.\n",
                i,buf);
            fprintf(data, "%d %lld %lld %lld\n", i, ktime, utime, utime -
        ktime);

        }
        */
    }

    /*
    for (int i = offset; i >= 0; i--) {
        char buf[4096];
        //lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, i);
        //unsigned long long temp = transfer1(buf,8);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
    */
    pthread_exit(NULL);
    close(fd);
    fclose(cost);
    fclose(data);

    return 0;
}
