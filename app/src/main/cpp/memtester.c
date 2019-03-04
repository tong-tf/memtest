/*
 * memtester version 4
 *
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 */

#include <android/log.h>
#define  LOG_TAG "tong"
#define  LOGI(fmt,args...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG, fmt, ##args)
#define  LOGD(fmt,args...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, fmt, ##args)
#define  LOGW(fmt,args...) __android_log_print(ANDROID_LOG_WARNING,LOG_TAG, fmt, ##args)
#define  LOGE(fmt,args...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG, fmt, ##args)
#define __version__ "4.3.0"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "types.h"
#include "sizes.h"
#include "tests.h"

#define EXIT_FAIL_NONSTARTER    0x01
#define EXIT_FAIL_ADDRESSLINES  0x02
#define EXIT_FAIL_OTHERTEST     0x04

struct test tests[] = {
    { "Random Value", test_random_value, 1<<2 },
    { "Compare XOR", test_xor_comparison, 1<<3 },
    { "Compare SUB", test_sub_comparison, 1<<4},
    { "Compare MUL", test_mul_comparison , 1<<5},
    { "Compare DIV",test_div_comparison  , 1<<6},
    { "Compare OR", test_or_comparison  , 1<<7},
    { "Compare AND", test_and_comparison , 1<<8 },
    { "Sequential Increment", test_seqinc_comparison , 1<<9 },
    { "Solid Bits", test_solidbits_comparison , 1<<10 },
    { "Block Sequential", test_blockseq_comparison , 1<<11 },
    { "Checkerboard", test_checkerboard_comparison  , 1<<12},
    { "Bit Spread", test_bitspread_comparison  , 1<<13},
    { "Bit Flip", test_bitflip_comparison  , 1<<14},
    { "Walking Ones", test_walkbits1_comparison  , 1<<15},
    { "Walking Zeroes", test_walkbits0_comparison  , 1<<16},
#ifdef TEST_NARROW_WRITES
    { "8-bit Writes", test_8bit_wide_random  , 1<<17},
    { "16-bit Writes", test_16bit_wide_random  , 1<<18},
#endif
    { NULL, NULL }
};

/* Sanity checks and portability helper macros. */
#ifdef _SC_VERSION
void check_posix_system(void) {
    if (sysconf(_SC_VERSION) < 198808L) {
        fprintf(stderr, "A POSIX system is required.  Don't be surprised if "
            "this craps out.\n");
        fprintf(stderr, "_SC_VERSION is %lu\n", sysconf(_SC_VERSION));
    }
}
#else
#define check_posix_system()
#endif

#ifdef _SC_PAGE_SIZE
int memtester_pagesize(void) {
    int pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1) {
        perror("get page size failed");
        exit(EXIT_FAIL_NONSTARTER);
    }
    printf("pagesize is %ld\n", (long) pagesize);
    return pagesize;
}
#else
int memtester_pagesize(void) {
    printf("sysconf(_SC_PAGE_SIZE) not supported; using pagesize of 8192\n");
    return 8192;
}
#endif

/* Some systems don't define MAP_LOCKED.  Define it to 0 here
   so it's just a no-op when ORed with other constants. */
#ifndef MAP_LOCKED
  #define MAP_LOCKED 0
#endif

/* Function declarations */
void usage(char *me);

/* Global vars - so tests have access to this information */
int use_phys = 0;
off_t physaddrbase = 0;

/* Function definitions */
void usage(char *me) {
    LOGI("\n"
            "Usage: %s [-p physaddrbase [-d device]] <mem>[B|K|M|G] [loops]\n",
            me);
    exit(EXIT_FAIL_NONSTARTER);
}




int do_test(int argc, char **argv, stage_callback callback) {
    ul loops, loop, i;
    size_t pagesize, wantraw, wantmb, wantbytes, wantbytes_orig, bufsize,
         halflen, count;
    char *memsuffix, *addrsuffix, *loopsuffix;
    ptrdiff_t pagesizemask;
    void volatile *buf, *aligned;
    char buff[128];
    ulv *bufa, *bufb;
    int do_mlock = 1, done_mem = 0;
    int exit_code = 0;
    int rv_item = 0;
    int memfd, opt, memshift;
    size_t maxbytes = -1; /* addressable memory, in bytes */
    size_t maxmb = (maxbytes >> 20) + 1; /* addressable memory, in MB */
    /* Device to mmap memory from with -p, default is normal core */
    char *device_name = "/dev/mem";
    struct stat statbuf;
    int device_specified = 0;
    char *env_testmask = 0;
    ul testmask = 0;

    int index = 1;
    for(i=0; i<argc; i++){
        LOGI("Args:  %d = %s", i, argv[i]);
    }
    LOGI("memtester version " __version__ " (%d-bit)\n", UL_LEN);
    LOGI("Copyright (C) 2001-2012 Charles Cazabon.\n");
    LOGI("Licensed under the GNU General Public License version 2 (only).\n");
    check_posix_system();
    pagesize = memtester_pagesize();
    pagesizemask = (ptrdiff_t) ~(pagesize - 1);
    LOGI("pagesizemask is 0x%tx\n", pagesizemask);

    /* If MEMTESTER_TEST_MASK is set, we use its value as a mask of which
       tests we run.
     */
    if (env_testmask = getenv("MEMTESTER_TEST_MASK")) {
        errno = 0;
        testmask = strtoul(env_testmask, 0, 0);
        if (errno) {
            LOGE("error parsing MEMTESTER_TEST_MASK %s: %s\n",
                    env_testmask, strerror(errno));
            usage(argv[0]); /* doesn't return */
        }
        LOGI("using testmask 0x%lx\n", testmask);
    }

    if (device_specified && !use_phys) {
        LOGE("for mem device, physaddrbase (-p) must be specified\n");
        usage(argv[0]); /* doesn't return */
    }


    errno = 0;
    wantraw = (size_t) strtoul(argv[index++], &memsuffix, 0);
    if (errno != 0) {
        LOGI(stderr, "failed to parse memory argument");
        usage(argv[0]); /* doesn't return */
    }
    switch (*memsuffix) {
        case 'G':
        case 'g':
            memshift = 30; /* gigabytes */
            break;
        case 'M':
        case 'm':
            memshift = 20; /* megabytes */
            break;
        case 'K':
        case 'k':
            memshift = 10; /* kilobytes */
            break;
        case 'B':
        case 'b':
            memshift = 0; /* bytes*/
            break;
        case '\0':  /* no suffix */
            memshift = 20; /* megabytes */
            break;
        default:
            /* bad suffix */
            usage(argv[0]); /* doesn't return */
    }
    wantbytes_orig = wantbytes = ((size_t) wantraw << memshift);
    wantmb = (wantbytes_orig >> 20);
    LOGI("wanted raw = %d, wantmb=%d", wantraw, wantmb);
    optind++;
    if (wantmb > maxmb) {
        LOGE("This system can only address %llu MB.\n", (ull) maxmb);
        exit(EXIT_FAIL_NONSTARTER);
    }
    if (wantbytes < pagesize) {
        LOGE("bytes %ld < pagesize %ld -- memory argument too large?\n",
                wantbytes, pagesize);
        exit(EXIT_FAIL_NONSTARTER);
    }


    errno = 0;
    loops = strtoul(argv[index++], &loopsuffix, 0);
    if (errno != 0) {
        LOGE("failed to parse number of loops");
        usage(argv[0]); /* doesn't return */
    }
    LOGI("loops = %d", loops);
    if (*loopsuffix != '\0') {
        LOGE("loop suffix %c\n", *loopsuffix);
        usage(argv[0]); /* doesn't return */
    }

    LOGI("want %lluMB (%llu bytes)\n", (ull) wantmb, (ull) wantbytes);
    sprintf(buff, "want %lluMB (%llu bytes)\n", (ull) wantmb, (ull) wantbytes);
    callback(buff);

    buf = NULL;

    if (use_phys) {
        memfd = open(device_name, O_RDWR | O_SYNC);
        if (memfd == -1) {
            LOGE("failed to open %s for physical memory: %s\n",
                    device_name, strerror(errno));
            exit(EXIT_FAIL_NONSTARTER);
        }
        buf = (void volatile *) mmap(0, wantbytes, PROT_READ | PROT_WRITE,
                                     MAP_SHARED | MAP_LOCKED, memfd,
                                     physaddrbase);
        if (buf == MAP_FAILED) {
            LOGE("failed to mmap %s for physical memory: %s\n",
                    device_name, strerror(errno));
            exit(EXIT_FAIL_NONSTARTER);
        }

        if (mlock((void *) buf, wantbytes) < 0) {
            LOGE("failed to mlock mmap'ed space\n");
            do_mlock = 0;
        }

        bufsize = wantbytes; /* accept no less */
        aligned = buf;
        done_mem = 1;
    }

    while (!done_mem) {
        while (!buf && wantbytes) {
            buf = (void volatile *) malloc(wantbytes);
            if (!buf) wantbytes -= pagesize;
        }
        bufsize = wantbytes;
        sprintf(buff, "got  %lluMB (%llu bytes)", (ull) wantbytes >> 20, (ull) wantbytes);
        callback(buff);
        LOGI("got  %lluMB (%llu bytes)", (ull) wantbytes >> 20, (ull) wantbytes);
        fflush(stdout);
        if (do_mlock) {
            LOGI(", trying mlock ...");
            fflush(stdout);
            if ((size_t) buf % pagesize) {
                /* printf("aligning to page -- was 0x%tx\n", buf); */
                aligned = (void volatile *) ((size_t) buf & pagesizemask) + pagesize;
                /* printf("  now 0x%tx -- lost %d bytes\n", aligned,
                 *      (size_t) aligned - (size_t) buf);
                 */
                bufsize -= ((size_t) aligned - (size_t) buf);
            } else {
                aligned = buf;
            }
            /* Try mlock */
            if (mlock((void *) aligned, bufsize) < 0) {
                switch(errno) {
                    case EAGAIN: /* BSDs */
                        LOGI("over system/pre-process limit, reducing...\n");
                        free((void *) buf);
                        buf = NULL;
                        wantbytes -= pagesize;
                        break;
                    case ENOMEM:
                        LOGI("too many pages, reducing...\n");
                        free((void *) buf);
                        buf = NULL;
                        wantbytes -= pagesize;
                        break;
                    case EPERM:
                        LOGI("insufficient permission.\n");
                        LOGI("Trying again, unlocked:\n");
                        do_mlock = 0;
                        free((void *) buf);
                        buf = NULL;
                        wantbytes = wantbytes_orig;
                        break;
                    default:
                        LOGI("failed for unknown reason.\n");
                        do_mlock = 0;
                        done_mem = 1;
                }
            } else {
                LOGI("locked OK.\n");
                done_mem = 1;
            }
        } else {
            done_mem = 1;
            printf("\n");
        }
    }

    if (!do_mlock) LOGI(stderr, "Continuing with unlocked memory; testing "
                           "will be slower and less reliable.\n");

    halflen = bufsize / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *) aligned;
    bufb = (ulv *) ((size_t) aligned + halflen);

    for(loop=1; ((!loops) || loop <= loops); loop++) {
        LOGI("Loop %lu", loop);
        if (loops) {
            printf("/%lu", loops);
        }
        fflush(stdout);
        rv_item = test_stuck_address(aligned, bufsize / sizeof(ul));
        if(rv_item != 0){
            exit_code |= EXIT_FAIL_ADDRESSLINES;
        }
        LOGI("  %-20s: %s", "Stuck Address", rv_item == 0 ? "OK": "FAIL");
        for (i=0;;i++) {
            if (!tests[i].name) break;
            /* If using a custom testmask, only run this test if the
               bit corresponding to this test was set by the user.
             */
            if (testmask && (!((1 << i) & testmask))) {
                continue;
            }

            rv_item = tests[i].fp(bufa, bufb, count);
            if(rv_item){
                exit_code |= tests[i].code;
            }
            LOGI("  %-20s: %s", tests[i].name,  rv_item == 0 ? "OK": "FAIL");
            sprintf(buff, "  %-20s: %s", tests[i].name,  rv_item == 0 ? "OK": "FAIL");
            callback(buff);
            fflush(stdout);
        }
        LOGI("\n");
        fflush(stdout);
    }
    if (do_mlock) munlock((void *) aligned, bufsize);
    LOGI("Done, exit_code=%d .\n", exit_code);
    fflush(stdout);
    return (exit_code);
}


int memtest(){
    int ac = 3;
    char *av[] = {"./a.out", "10M", "5"};
    return do_test(ac, av, NULL);
}


int memtest2(const char * size, const char * count, stage_callback callback){
    char repeat[16];
    char *av[3] = {"./aout", size, count};
    return do_test(3, av, callback);;
}

const char * memtest3(const char *size, const char *count, stage_callback callback){
    static char buf[512]; //  we should ensure, buff is long enough
    int rv = memtest2(size, count, callback);
    int i, n=0;
    n = sprintf(buf+n, "Stuck Address: ", rv & 0x2 ? "FAIL": "OK") ;
    for(i=0; tests[i].name; i++){
        n = sprintf(buf+n, "%s: %s\n", rv & (1<<(i+2)) ? "FAIL": "OK") ;
    }
    buf[n] = 0;
    return buf;
}