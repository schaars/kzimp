/*
 * ===========================================================================
 *
 *      Filename:  libperfctr.c
 *
 *      Description:  Marker API interface of module perfmon
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


/* #####   HEADER FILE INCLUDES   ######################################### */

#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>

#include <error.h>
#include <types.h>
#include <bstrlib.h>
#include <cpuid.h>
#include <tree.h>
#include <msr.h>
#include <timer.h>
#include <hashTable.h>
#include <registers.h>
#include <likwid.h>

//static LikwidResults*  likwid_results;
//static int likwid_numberOfRegions = 0;
//static int likwid_numberOfThreads = 0;

static volatile int nehalem_socket_lock[MAX_NUM_SOCKETS];
static int nehalem_processor_lookup[MAX_NUM_THREADS];

/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ######################### */

#define gettid() syscall(SYS_gettid)

/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ########### */

static int
getProcessorID(cpu_set_t* cpu_set)
{
    int processorId;

    for (processorId=0;processorId<MAX_NUM_THREADS;processorId++){
        if (CPU_ISSET(processorId,cpu_set))
        {  
            break;
        }
    }
    return processorId;
}

/* #####   FUNCTION DEFINITIONS  -  EXPORTED FUNCTIONS   ################## */

void
likwid_markerInit(void)
{
    TreeNode* socketNode;
    TreeNode* coreNode;
    TreeNode* threadNode;
    int socketId;

    cpuid_init();
    timer_init();
    msr_init();

    switch ( cpuid_info.model ) 
    {
        case NEHALEM_WESTMERE_M:
        case NEHALEM_WESTMERE:
        case NEHALEM_BLOOMFIELD:
        case NEHALEM_LYNNFIELD:
            for(int i=0; i<MAX_NUM_SOCKETS; i++) nehalem_socket_lock[i] = 0;

            socketNode = tree_getChildNode(cpuid_topology.topologyTree);
            while (socketNode != NULL)
            {
                socketId = socketNode->id;
                coreNode = tree_getChildNode(socketNode);

                while (coreNode != NULL)
                {
                    threadNode = tree_getChildNode(coreNode);

                    while (threadNode != NULL)
                    {
                        nehalem_processor_lookup[threadNode->id] = socketId;
                        threadNode = tree_getNextNode(threadNode);
                    }
                    coreNode = tree_getNextNode(coreNode);
                }
                socketNode = tree_getNextNode(socketNode);
            }
            break;
    }
}

/* File format 
 * 1 numberOfThreads numberOfRegions
 * 2 regionID:regionTag0
 * 3 regionID:regionTag1
 * 4 regionID threadID countersvalues(space sparated)
 * 5 regionID threadID countersvalues
 */
void
likwid_markerClose(void)
{
	int i,j,k;
    FILE *file = NULL;
    LikwidResults* results;
    int numberOfThreads;
    int numberOfRegions;

    hashTable_finalize(&numberOfThreads, &numberOfRegions, &results);

    file = fopen(getenv("LIKWID_FILEPATH"),"w");
    if (file != NULL)
    {
        fprintf(file,"%d %d\n",numberOfThreads,numberOfRegions);

        for (i=0; i<numberOfRegions; i++)
        {
            fprintf(file,"%d:%s\n",i,bdata(results[i].tag));
        }

        for (i=0; i<numberOfRegions; i++)
        {
            for (j=0; j<numberOfThreads; j++)
            {
                fprintf(file,"%d ",i);
                fprintf(file,"%d ",j);
                fprintf(file,"%u ",results[i].count[j]);
                fprintf(file,"%e ",results[i].time[j]);

                for (k=0; k<NUM_PMC; k++)
                {
                    fprintf(file,"%e ",results[i].counters[j][k]);
                }
                fprintf(file,"\n");
            }
        }
        fclose(file);
    }

    for (i=0;i<numberOfRegions; i++)
    {
        for (j=0;j<numberOfThreads; j++)
        {
            free(results[i].counters[j]);
        }
        free(results[i].time);
        free(results[i].counters);
    }

    free(results);
    msr_finalize();
}

void
likwid_markerStartRegion(char* regionTag)
{
    bstring tag = bfromcstralloc(100, regionTag);
    LikwidThreadResults* results = hashTable_get(tag);
    int cpu_id = results->coreId;

    results->count++;

    switch ( cpuid_info.family ) 
    {
        case P6_FAMILY:

            switch ( cpuid_info.model ) 
            {
                case PENTIUM_M_BANIAS:
                    break;

                case PENTIUM_M_DOTHAN:
                    break;

                case CORE_DUO:
                    break;

                case XEON_MP:

                case CORE2_65:

                case CORE2_45:

                    results->StartPMcounters[0] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0);
                    results->StartPMcounters[1] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1);
                    results->StartPMcounters[2] = (double) msr_read(cpu_id, MSR_PMC0);
                    results->StartPMcounters[3] = (double) msr_read(cpu_id, MSR_PMC1);
                    break;

                case NEHALEM_WESTMERE_M:

                case NEHALEM_WESTMERE:

                case NEHALEM_BLOOMFIELD:

                case NEHALEM_LYNNFIELD:

                    results->StartPMcounters[0] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0);
                    results->StartPMcounters[1] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1);
                    results->StartPMcounters[2] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR2);
                    results->StartPMcounters[3] = (double) msr_read(cpu_id, MSR_PMC0);
                    results->StartPMcounters[4] = (double) msr_read(cpu_id, MSR_PMC1);
                    results->StartPMcounters[5] = (double) msr_read(cpu_id, MSR_PMC2);
                    results->StartPMcounters[6] = (double) msr_read(cpu_id, MSR_PMC3);

                    if (nehalem_socket_lock[nehalem_processor_lookup[cpu_id]])
                    {
                        nehalem_socket_lock[nehalem_processor_lookup[cpu_id]] = 0;
                        results->StartPMcounters[7] = (double) msr_read(cpu_id, MSR_UNCORE_PMC0);
                        results->StartPMcounters[8] = (double) msr_read(cpu_id, MSR_UNCORE_PMC1);
                        results->StartPMcounters[9] = (double) msr_read(cpu_id, MSR_UNCORE_PMC2);
                        results->StartPMcounters[10] = (double) msr_read(cpu_id, MSR_UNCORE_PMC3);
                        results->StartPMcounters[11] = (double) msr_read(cpu_id, MSR_UNCORE_PMC4);
                        results->StartPMcounters[12] = (double) msr_read(cpu_id, MSR_UNCORE_PMC5);
                        results->StartPMcounters[13] = (double) msr_read(cpu_id, MSR_UNCORE_PMC6);
                        results->StartPMcounters[14] = (double) msr_read(cpu_id, MSR_UNCORE_PMC7);
                    }
                    break;

                case SANDYBRIDGE:

                case NEHALEM_EX:

                    results->StartPMcounters[0] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0);
                    results->StartPMcounters[1] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1);
                    results->StartPMcounters[2] = (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR2);
                    results->StartPMcounters[3] = (double) msr_read(cpu_id, MSR_PMC0);
                    results->StartPMcounters[4] = (double) msr_read(cpu_id, MSR_PMC1);
                    results->StartPMcounters[5] = (double) msr_read(cpu_id, MSR_PMC2);
                    results->StartPMcounters[6] = (double) msr_read(cpu_id, MSR_PMC3);
                    break;

                default:
                    ERROR_MSG(Unsupported Processor);
                    break;
            }
            break;

        case K8_FAMILY:

        case K10_FAMILY:

            results->StartPMcounters[0] = (double) msr_read(cpu_id, MSR_AMD_PMC0);
            results->StartPMcounters[1] = (double) msr_read(cpu_id, MSR_AMD_PMC1);
            results->StartPMcounters[2] = (double) msr_read(cpu_id, MSR_AMD_PMC2);
            results->StartPMcounters[3] = (double) msr_read(cpu_id, MSR_AMD_PMC3);
            break;

        default:
            ERROR_MSG(Unsupported Processor);
            break;
    }


    timer_startCycles(&(results->startTime));
}


void
likwid_markerStopRegion(char* regionTag)
{
    bstring tag = bfromcstralloc(100, regionTag);
    LikwidThreadResults* results;

    results = hashTable_get(tag);
    timer_stopCycles(&(results->startTime));
    int cpu_id = results->coreId;
    results->time += timer_printCyclesTime(&(results->startTime));

    switch ( cpuid_info.family ) 
    {
        case P6_FAMILY:

            switch ( cpuid_info.model ) 
            {
                case PENTIUM_M_BANIAS:
                    break;

                case PENTIUM_M_DOTHAN:
                    break;

                case CORE_DUO:
                    break;

                case XEON_MP:

                case CORE2_65:

                case CORE2_45:

                    results->PMcounters[0] += ((double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0) - results->StartPMcounters[0]);
                    results->PMcounters[1] += ((double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1) - results->StartPMcounters[1] );
                    results->PMcounters[2] += ((double) msr_read(cpu_id, MSR_PMC0) - results->StartPMcounters[2]);
                    results->PMcounters[3] += ((double) msr_read(cpu_id, MSR_PMC1) - results->StartPMcounters[3]);
                    break;

                case NEHALEM_WESTMERE_M:

                case NEHALEM_WESTMERE:

                case NEHALEM_BLOOMFIELD:

                case NEHALEM_LYNNFIELD:

                    results->PMcounters[0] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0) - results->StartPMcounters[0] ;
                    results->PMcounters[1] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1) - results->StartPMcounters[1] ;
                    results->PMcounters[2] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR2) - results->StartPMcounters[2] ;
                    results->PMcounters[3] += (double) msr_read(cpu_id, MSR_PMC0) - results->StartPMcounters[3] ;
                    results->PMcounters[4] += (double) msr_read(cpu_id, MSR_PMC1) - results->StartPMcounters[4] ;
                    results->PMcounters[5] += (double) msr_read(cpu_id, MSR_PMC2) - results->StartPMcounters[5] ;
                    results->PMcounters[6] += (double) msr_read(cpu_id, MSR_PMC3) - results->StartPMcounters[6] ;

                    if (nehalem_socket_lock[nehalem_processor_lookup[cpu_id]])
                    {
                        nehalem_socket_lock[nehalem_processor_lookup[cpu_id]] = 0;
                        results->PMcounters[7]  += (double) msr_read(cpu_id, MSR_UNCORE_PMC0) - results->StartPMcounters[7];
                        results->PMcounters[8]  += (double) msr_read(cpu_id, MSR_UNCORE_PMC1) - results->StartPMcounters[8];
                        results->PMcounters[9]  += (double) msr_read(cpu_id, MSR_UNCORE_PMC2) - results->StartPMcounters[9];
                        results->PMcounters[10] += (double) msr_read(cpu_id, MSR_UNCORE_PMC3) - results->StartPMcounters[10];
                        results->PMcounters[11] += (double) msr_read(cpu_id, MSR_UNCORE_PMC4) - results->StartPMcounters[11];
                        results->PMcounters[12] += (double) msr_read(cpu_id, MSR_UNCORE_PMC5) - results->StartPMcounters[12];
                        results->PMcounters[13] += (double) msr_read(cpu_id, MSR_UNCORE_PMC6) - results->StartPMcounters[13];
                        results->PMcounters[14] += (double) msr_read(cpu_id, MSR_UNCORE_PMC7) - results->StartPMcounters[14];
                    }
                    break;

                case SANDYBRIDGE:

                case NEHALEM_EX:

                    results->PMcounters[0] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR0) - results->StartPMcounters[0];
                    results->PMcounters[1] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR1) - results->StartPMcounters[1];
                    results->PMcounters[2] += (double) msr_read(cpu_id, MSR_PERF_FIXED_CTR2) - results->StartPMcounters[2];
                    results->PMcounters[3] += (double) msr_read(cpu_id, MSR_PMC0) - results->StartPMcounters[3];
                    results->PMcounters[4] += (double) msr_read(cpu_id, MSR_PMC1) - results->StartPMcounters[4];
                    results->PMcounters[5] += (double) msr_read(cpu_id, MSR_PMC2) - results->StartPMcounters[5];
                    results->PMcounters[6] += (double) msr_read(cpu_id, MSR_PMC3) - results->StartPMcounters[6];
                    break;

                default:
                    ERROR_MSG(Unsupported Processor);
                    break;
            }
            break;

        case K8_FAMILY:

        case K10_FAMILY:

            results->PMcounters[0] += (double) msr_read(cpu_id, MSR_AMD_PMC0) - results->StartPMcounters[0] ;
            results->PMcounters[1] += (double) msr_read(cpu_id, MSR_AMD_PMC1) - results->StartPMcounters[1] ;
            results->PMcounters[2] += (double) msr_read(cpu_id, MSR_AMD_PMC2) - results->StartPMcounters[2] ;
            results->PMcounters[3] += (double) msr_read(cpu_id, MSR_AMD_PMC3) - results->StartPMcounters[3] ;
            break;

        default:
            ERROR_MSG(Unsupported Processor);
            break;
    }
}

int  likwid_getProcessorId()
{
    cpu_set_t  cpu_set;
    CPU_ZERO(&cpu_set);
    sched_getaffinity(gettid(),sizeof(cpu_set_t), &cpu_set);

    return getProcessorID(&cpu_set);
}


int  likwid_processGetProcessorId()
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    sched_getaffinity(getpid(),sizeof(cpu_set_t), &cpu_set);

    return getProcessorID(&cpu_set);
}


int  likwid_threadGetProcessorId()
{
    cpu_set_t  cpu_set;
    CPU_ZERO(&cpu_set);
    sched_getaffinity(gettid(),sizeof(cpu_set_t), &cpu_set);

    return getProcessorID(&cpu_set);
}


#ifdef HAS_SCHEDAFFINITY
int  likwid_pinThread(int processorId)
{
    int ret;
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(processorId, &cpuset);
    ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    if (ret != 0)
    {   
        ERROR;
        return FALSE;
    }   

    return TRUE;
}
#endif


int  likwid_pinProcess(int processorId)
{
    int ret;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(processorId, &cpuset);
    ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    if (ret < 0)
    {
        ERROR;
        return FALSE;
    }   

    return TRUE;
}


