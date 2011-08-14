#include <stdlib.h>
#include <stdio.h>

#include <types.h>
#include <cpuid.h>

int main (int argc, char** argv)
{ 
    int i;

    cpuid_init();

    printf("name: %s \n",cpuid_info.name);
    printf("features: %s \n",cpuid_info.features);
    printf("family: %u \n",cpuid_info.family);
    printf("model: %u \n",cpuid_info.model);
    printf("stepping: %u \n\n",cpuid_info.stepping);

    printf("number of HWThreads: %u \n",cpuid_topology.numHWThreads);
    printf("number of sockets: %u \n",cpuid_topology.numSockets);
    printf("number of cores per socket: %u \n",cpuid_topology.numCoresPerSocket);
    printf("number of threads per core: %u \n\n",cpuid_topology.numThreadsPerCore);

    for (i=0; i< cpuid_topology.numHWThreads; i++)
    {
        printf("Thread %d\n",i);
        printf("id: %u \n",cpuid_topology.threadPool[i].threadId);
        printf("coreId: %u \n",cpuid_topology.threadPool[i].coreId);
        printf("packageId: %u \n",cpuid_topology.threadPool[i].packageId);
        printf("apicId: %u \n\n",cpuid_topology.threadPool[i].apicId);
    }
}
