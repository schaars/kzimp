/*
 * ===========================================================================
 *
 *      Filename:  msr-daemon.c
 *
 *      Description:  Implementation of msr access daemon.
 *
 *      Version:  <VERSION>
 *      Created:  <DATE>
 *
 *      Author:  Jan Treibig (jt), jan.treibig@gmail.com
 *      Company:  RRZE Erlangen
 *      Project:  likwid
 *      Copyright:  Copyright (c) 2011, Jan Treibig
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License, v2, as
 *      published by the Free Software Foundation
 *     
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *     
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * ===========================================================================
 */

/* #####   HEADER FILE INCLUDES   ######################################### */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/fsuid.h>

#include <msr_types.h>

/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ######################### */
#define SA struct sockaddr
#define str(x) #x

#define CHECK_ERROR(func, msg)  \
    if ((func) < 0) { syslog(LOG_ERR, "ERROR - [%s:%d] " str(msg) " - %s \n", __FILE__, __LINE__, strerror(errno)); }

#define EXIT_IF_ERROR(func, msg)  \
    if ((func) < 0) { syslog(LOG_ERR, "ERROR - [%s:%d] " str(msg) " - %s \n", __FILE__, __LINE__, strerror(errno)); stop_daemon(); }
 

#define CPUID                    \
    __asm__ volatile ("cpuid"    \
        : "=a" (eax),            \
          "=b" (ebx)             \
        : "0" (eax))


#define  P6_FAMILY        0x6U
#define  K10_FAMILY       0x10U
#define  K8_FAMILY        0xFU


/* #####   TYPE DEFINITIONS   ########### */
typedef int (*FuncPrototype)(uint32_t);

/* #####   VARIABLES  -  LOCAL TO THIS SOURCE FILE   ###################### */
static int sockfd = -1;
static char* filepath;
static const char* ident = "msrD";
static FuncPrototype allowed = NULL;
static volatile int isConnected = 0;

/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ########### */

static int
allowed_intel(uint32_t reg)
{
    if ( ((reg & 0x0F8U) == 0x0C0U) ||
         ((reg & 0xFF0U) == 0x180U) ||
         ((reg & 0xF00U) == 0x300U) ||
         (reg == 0x1A0)  ||
         (reg == 0x1A6))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int
allowed_amd(uint32_t reg)
{
    if ( (reg & 0xFFFFFFF0U) == 0xC0010000U)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


static void
msr_read(MsrDataRecord * msrd)
{
    int  fd;
    uint64_t data;
    char msr_file_name[64];
    uint32_t cpu = msrd->cpu;
    uint32_t reg = msrd->reg;

    msrd->errorcode = MSR_ERR_NOERROR;
    msrd->data = 0;

    if (!allowed(reg))
    {
        syslog(LOG_ERR, "attempt to read from restricted register %x", reg);
        msrd->errorcode = MSR_ERR_RESTREG;
        return;
    }

    snprintf(msr_file_name, sizeof(msr_file_name), "/dev/cpu/%u/msr", cpu);
    fd = open(msr_file_name, O_RDONLY);
    
    if (fd < 0)
    {
        syslog(LOG_ERR, "Failed to open msr device file on core %u", cpu);
        msrd->errorcode = MSR_ERR_OPENFAIL;
        return;
    }

    if (pread(fd, &data, sizeof(data), reg) != sizeof(data)) 
    {
        syslog(LOG_ERR, "Failed to read data from msr device file on core %u", cpu);
        msrd->errorcode = MSR_ERR_RWFAIL;
        return;
    }

    (void) close(fd);
    msrd->data = data;
    return;
}

static void 
msr_write(MsrDataRecord * msrd)
{
    int  fd;
    char msr_file_name[64];
    uint32_t cpu = msrd->cpu;
    uint32_t reg = msrd->reg;
    uint64_t data = msrd->data;
    
    msrd->errorcode = MSR_ERR_NOERROR;

    if (!allowed(reg))
    {
        syslog(LOG_ERR, "attempt to write to restricted register %x", reg);
        msrd->errorcode = MSR_ERR_RESTREG;
        return;
    }

    snprintf(msr_file_name, sizeof(msr_file_name), "/dev/cpu/%u/msr", cpu);
    fd = open(msr_file_name, O_WRONLY);

    if (fd < 0) 
    {
        syslog(LOG_ERR, "Failed to open msr device file on core %u", cpu);
        msrd->errorcode = MSR_ERR_OPENFAIL;
        return;
    }

    if (pwrite(fd, &data, sizeof data, reg) != sizeof(data)) 
    {
        syslog(LOG_ERR, "Failed to write data from msr device file on core %u", cpu);
        msrd->errorcode = MSR_ERR_RWFAIL;
        return;
    }

    (void) close(fd);
}

static void stop_daemon(void)
{
    syslog(LOG_NOTICE, "daemon stopped");

    if (sockfd != -1) 
    {
        CHECK_ERROR(close(sockfd), socket close failed);
    }

    free(filepath);
    closelog();
    exit(EXIT_SUCCESS);
}

static void Signal_Handler(int sig)
{
    if (sig == SIGPIPE)
    {
        syslog(LOG_NOTICE, "SIGPIPE? client crashed?!");
        stop_daemon();
    }
    /* For SIGALRM we just return - we're just here to create a EINTR */
}

static void daemonize(int* parentPid)
{
    pid_t pid, sid;

    *parentPid = getpid();

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0)
    {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) 
    {
        syslog(LOG_ERR, "chdir failed:  %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    {
        FILE* ret;
        ret = freopen( "/dev/null", "r", stdin);
        ret = freopen( "/dev/null", "w", stdout);
        ret = freopen( "/dev/null", "w", stderr);
    }
}

/* #####  MAIN FUNCTION DEFINITION   ################## */

int main(void) 
{
    int ret;
    int connfd;
    pid_t pid;
    struct sockaddr_un  addr1;
    socklen_t socklen;
    MsrDataRecord msrData;
    mode_t oldumask;

    openlog(ident, 0, LOG_USER);

    daemonize(&pid);

    {
        uint32_t  eax = 0x00;
        uint32_t  ebx = 0x00;
        int isIntel = 1;
        CPUID;
        if (ebx == 0x68747541U)
        {
            isIntel = 0;
        }

        eax = 0x01;
        CPUID;
        uint32_t family = ((eax>>8)&0xFU) + ((eax>>20)&0xFFU);

        switch (family)
        {
            case P6_FAMILY:
                allowed = allowed_intel;
                break;
            case K8_FAMILY:
                if (isIntel)
                {
                    syslog(LOG_ERR,
                           "ERROR - [%s:%d] - Netburst architecture is not supported! Exiting! \n",
                           __FILE__,__LINE__);
                    exit(EXIT_FAILURE);
                }
            case K10_FAMILY:
                allowed = allowed_amd;
                break;
            default:
                syslog(LOG_ERR, "ERROR - [%s:%d] - Unsupported processor. Exiting!  \n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
        }
    }

    /* setup filename for socket */
    filepath = (char*) calloc(sizeof(addr1.sun_path), 1);
    snprintf(filepath, sizeof(addr1.sun_path),"/tmp/likwid-%d",pid);

    /* get a socket */
    EXIT_IF_ERROR(sockfd = socket(AF_LOCAL, SOCK_STREAM, 0), socket failed);

    /* initialize socket data structure */
    bzero(&addr1, sizeof(addr1));
    addr1.sun_family = AF_LOCAL;
    strncpy(addr1.sun_path, filepath, (sizeof(addr1.sun_path) - 1)); /* null terminated by the bzero() above! */

    /* Change the file mode mask so only the calling user has access
     * and switch the user/gid with which the following socket creation runs. */
    oldumask = umask(077);
    CHECK_ERROR(setfsuid(getuid()), setfsuid failed);

    /* bind and listen on socket */
    EXIT_IF_ERROR(bind(sockfd, (SA*) &addr1, sizeof(addr1)), bind failed);
    EXIT_IF_ERROR(listen(sockfd, 1), listen failed);
    EXIT_IF_ERROR(chmod(filepath, S_IRUSR|S_IWUSR), chmod failed);

    /* Restore the old umask and fs ids. */
    (void) umask(oldumask);
    CHECK_ERROR(setfsuid(geteuid()), setfsuid failed);

    socklen = sizeof(addr1);

    { /* Init signal handler */
        struct sigaction sia;
        sia.sa_handler = Signal_Handler;
        sigemptyset(&sia.sa_mask);
        sia.sa_flags = 0;
        sigaction(SIGALRM, &sia, NULL);
        sigaction(SIGPIPE, &sia, NULL);
    }

    /* setup an alarm to stop the daemon if there is no connect */
    alarm(15U);

    /* accept one connect from one client */
    if ((connfd = accept(sockfd, (SA*) &addr1, &socklen)) < 0)
    {
        if (errno == EINTR)
        {
            syslog(LOG_ERR, "exiting due to timeout - no client connected after 15 seconds.");
        }
        else
        {
            syslog(LOG_ERR, "accept() failed:  %s", strerror(errno));
        }
        CHECK_ERROR(unlink(filepath), unlink of socket failed);
        exit(EXIT_FAILURE);
    }

    isConnected = 1;
    alarm(0);
    CHECK_ERROR(unlink(filepath), unlink of socket failed);
    syslog(LOG_NOTICE, "daemon started");
    pid = getpid();

    for ( ; ; )
    {
        ret = read(connfd, (void*) &msrData, sizeof(MsrDataRecord));

        if (ret < 0)
        { 
            syslog(LOG_ERR, "ERROR - [%s:%d] read from client failed  - %s \n", __FILE__, __LINE__, strerror(errno));
            stop_daemon();
        }
        else if (ret == 0)
        {
            syslog(LOG_ERR, "ERROR - [%s:%d] zero read", __FILE__, __LINE__);
            stop_daemon();
        }
        else if (ret != sizeof(MsrDataRecord))
        {
            syslog(LOG_ERR, "ERROR - [%s:%d] unaligned read", __FILE__, __LINE__);
            stop_daemon();
        }

        if (msrData.type == MSR_READ)
        {
            msr_read(&msrData);
        }
        else if (msrData.type == MSR_WRITE)
        {
            msr_write(&msrData);
            msrData.data = 0x0ULL;
        }
        else if (msrData.type == EXIT)
        {
            stop_daemon();
        }
        else
        {
            syslog(LOG_ERR, "unknown msr access type  %d", msrData.type);
            msrData.errorcode = MSR_ERR_UNKNOWN;
        }

        EXIT_IF_ERROR(write(connfd, (void*) &msrData, sizeof(MsrDataRecord)), write failed);
    }

    /* never reached */
    return EXIT_SUCCESS;
}

