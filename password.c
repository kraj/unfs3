/*
 * UNFS3 mount password support routines
 * (C) 2004, Peter Astrand <peter@cendio.se>
 * see file LICENSE for license details
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>           /* gettimeofday */
#include <sys/times.h>          /* times */
#include "md5.h"

void
gen_nonce(char *nonce)
{
    struct stat st;
    struct tms tmsbuf;
    md5_state_t state;
    unsigned int *arr;
    int bytes_read, fd;

    if (((fd = open("/dev/urandom", O_RDONLY)) != -1)
        || ((fd = open("/dev/random", O_RDONLY)) != -1)) {
        bytes_read = read(fd, nonce, 32);
        close(fd);
        if (bytes_read == 32)
            return;
    }

    /* No /dev/random; do it by hand */
    arr = (unsigned int *) nonce;
    stat("/tmp", &st);
    arr[0] = st.st_mtime;
    arr[1] = st.st_atime;
    arr[2] = st.st_ctime;
    arr[3] = times(&tmsbuf);
    arr[4] = tmsbuf.tms_cutime;
    arr[5] = tmsbuf.tms_cstime;
    gettimeofday((struct timeval *) &arr[6], NULL);

    md5_init(&state);
    md5_append(&state, nonce, 32);
    md5_finish(&state, nonce);
}

static unsigned char
nibble_as_hexchar(unsigned char c)
{
    if (c <= 9)
        return c + '0';

    return c - 10 + 'a';
}

static void
hexify(unsigned char digest[16], unsigned char hexdigest[32])
{
    int i, j;

    for (i = j = 0; i < 16; i++) {
        char c;
        /* The first four bits */
        c = (digest[i] >> 4) & 0xf;
        hexdigest[j++] = nibble_as_hexchar(c);
        /* The next four bits */
        c = (digest[i] & 0xf);
        hexdigest[j++] = nibble_as_hexchar(c);
    }
}

/* Handle mount commands:
 * Advance dpath to first slash
 * Copy command arguments to arg. 
*/
void
mnt_cmd_argument(char **dpath, const char *cmd, char *arg, size_t maxlen)
{
    char *slash;

    *dpath += strlen(cmd);
    strncpy(arg, *dpath, maxlen);
    arg[maxlen] = '\0';

    slash = strchr(arg, '/');
    if (slash != NULL)
        *slash = '\0';

    *dpath += strlen(arg);
}

void otp_digest(char nonce[32], 
		unsigned char *password, 
		unsigned char hexdigest[32])
{
    md5_state_t state;
    unsigned char digest[16];
    
    /* Calculate the digest, in the same way as the client did */
    md5_init(&state);
    md5_append(&state, nonce, 32);
    md5_append(&state, password, strlen(password));
    md5_finish(&state, digest);
    hexify(digest, hexdigest);
}