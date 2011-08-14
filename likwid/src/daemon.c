/*
 * ===========================================================================
 *
 *      Filename:  daemon.c
 *
 *      Description:  C Module implementing a daemon time loop
 *
 *      Version:  2.2
 *      Created:  14.06.2011
 *
 *      Author:  Jan Treibig (jt), jan.treibig@gmail.com
 *      Company:  RRZE Erlangen
 *      Project:  likwid
 *      Copyright:  Copyright (c) 2010, Jan Treibig
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include <timer.h>
#include <perfmon.h>
#include <daemon.h>

static int daemon_run = 0;
static bstring eventString;


void
daemon_init(bstring str)
{
    eventString = bstrcpy(str);
    signal(SIGINT, daemon_stop);
    signal(SIGUSR1, daemon_interrupt);

}

void
daemon_start(struct timespec interval)
{
    daemon_run = 1;
    perfmon_startCounters();

    while (1)
    {
        if (daemon_run)
        {
            perfmon_readCounters();
            perfmon_logCounterResults((double) interval.tv_sec + (double) interval.tv_nsec * 1.0E-9 );
        }
        nanosleep( &interval, NULL);
    }
}

void
daemon_stop(int sig)
{
    printf("DAEMON:  EXIT on %d\n", sig);
    perfmon_stopCounters();
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
}

void
daemon_interrupt(int sig)
{
    if (daemon_run)
    {
        perfmon_stopCounters();
        daemon_run = 0;
        printf("DAEMON:  STOP on %d\n",sig);
    }
    else
    {
        perfmon_setupEventSet(eventString);
        perfmon_startCounters();
        daemon_run = 1;
        printf("DAEMON:  START\n");
    }
}


