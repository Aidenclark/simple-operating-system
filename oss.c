#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <time.h>
#include "shared.h"
#include "queue.h"

#define MAXPRO 18

/* ========================================OSS========================================================

* You can create fixed sizedarrays for page tables, assuming that each process will have a requirement 
of less than 32K memory, with each page being 1K.

* After the resources have been set up,forka user process at random times 
(between 1 and 500 milliseconds of your logicalclock)

*  18 processes is a hard limit and you should implement it using a#definemacro.  
Thus,  if a user specifies an actual number of processes as 30,  your hard limit will still limit 
it to no more than 18processes at any time in the system

* Each request for disk read/write takes about 14ms to be fulfilled.

==================================================================================================== */

/* ====================================================================================================
* Shared memory setups and semaphores
==================================================================================================== */

int shmid;
int shmid_mem;
sm *ptr;
sem_t *sem;

/* ====================================================================================================
* Message queue structs
==================================================================================================== */
struct message
{
    long messageType;
    char mtext[512];
};

struct memory *mem;
struct memory memstruct;

struct message msg;

/* ====================================================================================================
* Int declarations
==================================================================================================== */
int faults;
int requests;
int pageNUM[MAXPRO][32];
int messageQUEUE;
int alliveProcess[20];
int pidNUM = 0;
int termed = 0;
int secondsIter = 1;
int lines = 0;
int maxProcesses = 20; // default value 

/* ====================================================================================================
* Function prototypes
==================================================================================================== */
void Statistics();
void MemoryLayout();
void setupSharedMemory();
void detachSharedMemory();
void signalErrors();
void incClock(struct time *time, int sec, int ns);
int pageLocation(int, int);
void setPageNumber();
int swapFrame();
int findFrame();
void sendPage(int, int, int);
void MemoryReset(int);

int timer = 2;
int memoryAccess = 0;

/* ====================================================================================================
* Logfile
==================================================================================================== */
char outputFileName[] = "logfile";
FILE *fp;

/* ====================================================================================================
* Wroking function for OSS
==================================================================================================== */
int main(int argc, char *argv[])
{
    int c;
    int swapframe = -1;
    int sendNum = -1;
    int unblockNum = -1;
    int frameNumResult = 0;
    int findframe = 0;
    int i = 0;

    /* ====================================================================================================
    * Getopt and pass the command line arguments
    ==================================================================================================== */
    while ((c = getopt(argc, argv, "m:p:t:h")) != EOF)
    {
        switch (c)
        {
        case 'h':
            printf("====================================Help Menu===================================\n"); 
            printf("Invocation: ./oss [-h] [-p x ] [-m x] [-t x]\n");
            printf("================================================================================\n");
            printf("       -h -->         Prints the help menu and then terminates. \n");
            printf("       -p -->         Changes the number of processes (Default is 20)\n");
            printf("       -m -->         Indicate the memory request schemes by typing 0 or 1 (Default 0)\n");
            printf("       -t -->         Change the timer length (Default of 2 seconds)\n");
            printf("=====================================================================================\n");
            exit(0);
            break;
        case 'm':
            memoryAccess = atoi(optarg);
            break;
        case 'p':
            maxProcesses = atoi(optarg);
            break;
        case 't':
            timer = atoi(optarg);
            break;
        default:
            return -1;
        }
    }

    fp = fopen(outputFileName, "w");

    //int maxProcesses = 100;
    srand(time(NULL));
    int count = 0;
    pid_t cpid;

    int pid = 0;

    time_t t;
    srand((unsigned)time(&t));
    int totalCount = 0;

    /* ====================================================================================================
    * Spawn all of the processes
    ==================================================================================================== */
    for (i = 0; i < 18; i++)
    {
        alliveProcess[i] = i;
    }

    /* ====================================================================================================
    * Signal intterupts
    ==================================================================================================== */
    if (signal(SIGINT, signalErrors) == SIG_ERR) // ctrl-c
    {
        exit(0);
    }

    if (signal(SIGALRM, signalErrors) == SIG_ERR) // if program goes past second alarm
    {
        exit(0);
    }

    /* ====================================================================================================
    * setupSharedMemory shared memory, create a queue for the blocked processes and set up page numbers
    ==================================================================================================== */
    setupSharedMemory();

    create();

    setPageNumber();

    /* ====================================================================================================
    * Initilizise the array for the second method
    ==================================================================================================== */
    float processWeight;
    for (i = 1; i <= 32; i++)
    {
        processWeight = 1 / (float)i;
        ptr->resource.weights[i - 1] = processWeight;
    }

    if (memoryAccess == 0)
    {
        printf("Memory Access Method: 0\n");
        ptr->resource.memoryType = 0;
    }
    else
    {
        printf("Memory Access Method: 1\n");
        ptr->resource.memoryType = 1;
    }

    struct time randFork;

    /* ====================================================================================================
    * Set the alarm
    ==================================================================================================== */
    alarm(timer);

    /* ====================================================================================================
    * Working loop: run for max process or until they are all dead
    ==================================================================================================== */
    while (totalCount < maxProcesses || count > 0)
    {

        if (waitpid(cpid, NULL, WNOHANG) > 0)
        {
            count--;
        }
        incClock(&ptr->time, 0, 10000);
        int nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
        incClock(&randFork, 0, nextFork);

        /* ====================================================================================================
        * Run as long as there is less than 20
        ==================================================================================================== */
        if (count < 18 && ptr->time.ns < nextFork)
        {
            /* ====================================================================================================
            * Change fork each iteration
            ==================================================================================================== */
            sem_wait(sem);
            randFork.s = ptr->time.s;
            randFork.ns = ptr->time.ns;
            sem_post(sem);
            nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
            incClock(&randFork, 0, nextFork);

            /* ====================================================================================================
            * Run until all the processes terminate
            ==================================================================================================== */
            int l;
            for (l = 0; l < 18; l++)
            {
                if (alliveProcess[l] == -1)
                {
                    termed++;
                }
            }

            /* ====================================================================================================
            * Exit and detach memory
            ==================================================================================================== */
            if (termed == 18)
            {
                Statistics();
                detachSharedMemory();
                return 0;
            }
            else
            {
                termed = 0;
            }

            if (alliveProcess[pidNUM] != -1)
            {
                pid = alliveProcess[pidNUM];
            }
            else
            {
                int s = pidNUM;
                for (s = pidNUM; s < 18; s++)
                {
                    if (alliveProcess[s] == -1)
                    {
                        pidNUM++;
                    }
                    else
                    {
                        break;
                    }
                }

                pid = alliveProcess[pidNUM];
            }

            /* ====================================================================================================
            * Fork to user.c
            ==================================================================================================== */
            cpid = fork();

            totalCount++;
            count++;

            if (cpid == 0)
            {
                char passPid[10];
                sprintf(passPid, "%d", pid);
                execl("./user", "user", NULL);
                exit(0);
            }

            unblockNum = getProcess();

            if (unblockNum != -1)
            { // if the blocked queue isn't empty then remove from queue
                removeProcess(unblockNum);
            }

            if (msgrcv(messageQUEUE, &msg, sizeof(msg) + 1, 99, 0) == -1)
            {
                perror("msgrcv");
            }

            /* ====================================================================================================
            * If the user wants to write
            ==================================================================================================== */
            if (strcmp(msg.mtext, "WRITE") == 0)
            {
                msgrcv(messageQUEUE, &msg, sizeof(msg), 99, 0); // address message

                int write = atoi(msg.mtext);
                ptr->resource.count += 1;

                if (ptr->resource.memoryType == 0) // address avaliable? 
                {
                    frameNumResult = pageLocation(pid, write / 1024);
                }
                if (ptr->resource.memoryType == 1)
                {
                    frameNumResult = pageLocation(pid, (write / 1024) / 2);
                }
                incClock(&ptr->time, 0, 14000000);
                fprintf(fp, "Master: P%d Requesting write of address %d at %d:%d\n", pid, write, ptr->time.s, ptr->time.ns);
                requests++;


                if (frameNumResult != -1)
                { // grant if available 
                    incClock(&ptr->time, 0, 10);
                    mem->referenceBit[frameNumResult] = 1;
                    mem->dirty[frameNumResult] = 1;
                    incClock(&ptr->time, 0, 10000);
                    fprintf(fp, "Address %d is in frame %d, writing data to frame at time %d:%d\n", write, frameNumResult, ptr->time.s, ptr->time.ns);
                }
                /* ====================================================================================================
                * Page fault occurs
                ==================================================================================================== */
                else
                {
                    faults++;
                    findframe = findFrame();
                    fprintf(fp, "Address %d is not in a frame, pagefault\n", write);

                    if (findframe == -1)
                    { // select page to replace if memory is full 
                        swapframe = swapFrame();
                        if (ptr->resource.memoryType == 0)
                        {
                            sendPage(pageNUM[pid][write / 1024], swapframe, 0);
                            mem->pagetable[pid][write / 1024] = swapframe;
                        }
                        if (ptr->resource.memoryType == 1)
                        {
                            sendPage(pageNUM[pid][(write / 1024) / 2], swapframe, 0);
                            mem->pagetable[pid][(write / 1024) / 2] = swapframe;
                        }

                        fprintf(fp, "Clearing frame %d and swapping in P%d\n", swapframe, pid);
                        fprintf(fp, "Dirty bit of frame %d is set, adding time to clock\n", swapframe);
                    }

                    else
                    { // place page in open frame
                        if (ptr->resource.memoryType == 0)
                        {
                            sendNum = pageNUM[pid][write / 1024];
                            sendPage(sendNum, findframe, 0);
                            mem->pagetable[pid][write / 1024] = findframe;
                        }
                        if (ptr->resource.memoryType == 1)
                        {
                            sendNum = pageNUM[pid][(write / 1024) / 2];
                            sendPage(sendNum, findframe, 0);
                            mem->pagetable[pid][(write / 1024) / 2] = findframe;
                        }
                        fprintf(fp, "Clearing frame %d and swapping in P%d page %d\n", findframe, pid, sendNum);
                        fprintf(fp, "Dirty bit of frame %d is set, adding time to clock\n", findframe);
                    }
                    /* ====================================================================================================
                    * Add process to the queue 
                    ==================================================================================================== */
                    addProcess(pid);
                }
            }
            /* ====================================================================================================
            * If the user wants a read request
            ==================================================================================================== */
            if (strcmp(msg.mtext, "REQUEST") == 0)
            {
                msgrcv(messageQUEUE, &msg, sizeof(msg), 99, 0); // address from user.c 
                int request = atoi(msg.mtext);
                ptr->resource.count += 1;

                if (ptr->resource.memoryType == 0)
                { // can request be granted? 
                    frameNumResult = pageLocation(pid, request / 1024);
                }
                if (ptr->resource.memoryType == 1)
                {
                    frameNumResult = pageLocation(pid, (request / 1024) / 2);
                }
                incClock(&ptr->time, 0, 14000000);
                fprintf(fp, "Master: P%d Requesting read of address %d at %d:%d\n", pid, request, ptr->time.s, ptr->time.ns);
                requests++;

                if (frameNumResult != -1)
                { // read if granted
                    incClock(&ptr->time, 0, 10);
                    mem->referenceBit[frameNumResult] = 1;
                    fprintf(fp, "Address %d is in frame %d, giving data to P%d at time %d:%d\n", request, frameNumResult, pid, ptr->time.s, ptr->time.ns);
                }
                /* ====================================================================================================
                * Page fault occurs and empty frame is filled
                ==================================================================================================== */

                else
                {
                    faults++;
                    findframe = findFrame();
                    fprintf(fp, "Address %d is not in a frame, pagefault\n", request);

                    if (findframe == -1)
                    { // if memery full then replace page
                        swapframe = swapFrame();
                        if (ptr->resource.memoryType == 0)
                        {
                            sendPage(pageNUM[pid][request / 1024], swapframe, 0);
                            mem->pagetable[pid][request / 1024] = swapframe;
                        }
                        if (ptr->resource.memoryType == 1)
                        {
                            sendPage(pageNUM[pid][(request / 1024) / 2], swapframe, 0);
                            mem->pagetable[pid][(request / 1024) / 2] = swapframe;
                        }

                        fprintf(fp, "Clearing frame %d and swapping in P%d\n", swapframe, pid);
                    }

                    else
                    { // put page in empty frame
                        if (ptr->resource.memoryType == 0)
                        {
                            sendNum = pageNUM[pid][request / 1024];
                            sendPage(sendNum, findframe, 0);
                            mem->pagetable[pid][request / 1024] = findframe;
                        }
                        if (ptr->resource.memoryType == 1)
                        {
                            sendNum = pageNUM[pid][(request / 1024) / 2];
                            sendPage(sendNum, findframe, 0);
                            mem->pagetable[pid][(request / 1024) / 2] = findframe;
                        }

                        fprintf(fp, "Clearing frame %d and swapping in P%d page %d\n", findframe, pid, sendNum);
                    }
                    /* ====================================================================================================
                    * Add process to the queue 
                    ==================================================================================================== */

                    addProcess(pid);
                }
            }

            /* ====================================================================================================
            * If a process needs to terminate then update the acttive arrary and release memory 
            ==================================================================================================== */

            if (strcmp(msg.mtext, "TERMINATED") == 0)
            {
                fprintf(fp, "Master: Terminating P%d at %d:%d\n", pid, ptr->time.s, ptr->time.ns);
                alliveProcess[pid] = -1;
                MemoryReset(pid);
            }

            if (ptr->time.s == secondsIter)
            { // print memoery layout
                secondsIter += 1;
                MemoryLayout();
            }

            if (pidNUM < 17)
            {
                pidNUM++;
            }
            else
            {
                pidNUM = 0;
            }
        }
    }
    /* ====================================================================================================
    * Print the stats and detachSharedMemory the memory
    ==================================================================================================== */

    Statistics();
    detachSharedMemory();
    return 0;
}

/* ====================================================================================================
* Print current memory layour
==================================================================================================== */
void MemoryLayout()
{
    fprintf(fp, "\nCurrent Memory layout at time %d:%d\n", ptr->time.s, ptr->time.ns);
    fprintf(fp, "\t     Occupied\tRef\t   DirtyBit\n");

    int i;
    for (i = 0; i < 256; i++)
    {
        fprintf(fp, "Frame %d:\t", i + 1);

        if (mem->bitvector[i] == 0)
        {
            fprintf(fp, "No\t");
        }
        else
        {
            fprintf(fp, "Yes\t");
        }

        if (mem->bitvector[i] == 1)
        {
            if (mem->referenceBit[i] == 0)
            {
                fprintf(fp, "0\t");
            }
            else
            {
                fprintf(fp, "1\t");
            }
        }
        else
        {
            fprintf(fp, "0\t");
        }
        if (mem->dirty[i] == 0)
        {
            fprintf(fp, "0\t");
        }
        else
        {
            fprintf(fp, "1\t");
        }
        fprintf(fp, "\n");
    }
}

/* ====================================================================================================
* Print stats
==================================================================================================== */
void Statistics()
{
    printf("===================Statisitics======================\n");
    printf("Number of memory accesses per second: %f\n", ((float)(requests) / ((float)(ptr->time.s))));
    printf("Number of page faults per memory access: %f\n", ((float)(faults) / (float)requests));
    printf("Average memory access speed: %f\n", (((float)(ptr->time.s) + ((float)ptr->time.ns / (float)(1000000000))) / ((float)requests)));
    printf("Number of seg faults per memory access: %f\n", ((float)(faults)));
    printf("====================================================\n\n");

    fprintf(fp, "\nNumber of memory accesses per second: %f\n", ((float)(requests) / ((float)(ptr->time.s))));
    fprintf(fp, "Number of page faults per memory access: %f\n", ((float)(faults) / (float)requests));
    fprintf(fp, "Average memory access speed: %f\n\n", (((float)(ptr->time.s) + ((float)ptr->time.ns / (float)(1000000000))) / ((float)requests)));
    fprintf(fp, "Number of seg faults per memory access: %f\n", ((float)(faults)));
}

/* ====================================================================================================
* Clear the frame and reset memory when termination occurs
==================================================================================================== */
void MemoryReset(int id)
{
    int i;
    int frame;
    int page;
    for (i = 0; i < 32; i++)
    {
        if (mem->pagetable[id][i] != -1)
        {
            frame = mem->pagetable[id][i];
            page = pageNUM[id][i];
            mem->referenceBit[frame] = 0;
            mem->dirty[frame] = 0;
            mem->bitvector[frame] = 0;
            mem->frame[frame] = -1;
            mem->pagetable[id][i] = -1;
            mem->pagelocation[page] = -1;
        }
    }
}

/* ====================================================================================================
* Return the next empty frame
==================================================================================================== */
int findFrame()
{
    int i;
    for (i = 0; i < 256; i++)
    {
        if (mem->bitvector[i] == 0)
        {
            return i;
        }
    }
    return -1;
}

/* ====================================================================================================
* Change information about page
==================================================================================================== */
void sendPage(int pageNum, int frameNum, int dirtybit)
{
    mem->referenceBit[frameNum] = 1;
    mem->dirty[frameNum] = dirtybit;
    mem->bitvector[frameNum] = 1;
    mem->frame[frameNum] = pageNum;
    mem->pagelocation[pageNum] = frameNum;
}

/* ====================================================================================================
* Find page and replace it
==================================================================================================== */
int swapFrame()
{
    int frameNum;
    while (1)
    {

        if (mem->referenceBit[mem->referenceStat] == 1)
        {
            mem->referenceBit[mem->referenceStat] = 0;
            if (mem->referenceStat == 255)
            {
                mem->referenceStat = 0;
            }
            else
            {
                mem->referenceStat++;
            }
        }

        else
        {
            frameNum = mem->referenceStat;
            if (mem->referenceStat == 255)
            {
                mem->referenceStat = 0;
            }
            else
            {
                mem->referenceStat++;
            }
            return frameNum;
        }
    }
}

/* ====================================================================================================
* Return the frame number if there or return -1
==================================================================================================== */
int pageLocation(int u_pid, int u_num)
{
    int i;
    int pageNum;
    int frameNum;

    pageNum = pageNUM[u_pid][u_num];

    frameNum = mem->pagelocation[pageNum];

    if (frameNum == -1)
    {
        return -1;
    }

    if (mem->frame[frameNum] == pageNum)
    {
        return frameNum;
    }

    return -1;
}

/* ====================================================================================================
* Set page numbers
==================================================================================================== */
void setPageNumber()
{
    int process;
    int num;
    int i;
    for (process = 0; process < 18; process++)
    {
        for (num = 0; num < 32; num++)
        {
            mem->pagetable[process][num] = -1;
            pageNUM[process][num] = process * 32 + num;
        }
    }
    for (num = 0; num < 576; num++)
    {
        mem->pagelocation[num] = -1;
    }
    for (i = 0; i < 256; i++)
    {
        mem->frame[i] = -1;
    }
}

/* ====================================================================================================
* Print clock and protect with semaphore
==================================================================================================== */
void incClock(struct time *time, int sec, int ns)
{
    sem_wait(sem);
    time->s += sec;
    time->ns += ns;

    while (time->ns >= 1000000000)
    {
        time->ns -= 1000000000;
        time->s++;
    }
    sem_post(sem);
}

/* ====================================================================================================
* Set up the share memory
==================================================================================================== */
void setupSharedMemory()
{

    shmid_mem = shmget(4020014, sizeof(memstruct), 0777 | IPC_CREAT);
    if (shmid_mem == -1)
    {
        exit(0);
    }
    mem = (struct memory *)shmat(shmid_mem, NULL, 0);
    if (mem == (struct memory *)(-1))
    {
        exit(0);
    }


    if ((shmid = shmget(9784, sizeof(sm), IPC_CREAT | 0600)) < 0)
    {// shared menory segment
        exit(0);
    }

    if ((messageQUEUE = msgget(4020015, 0777 | IPC_CREAT)) == -1)
    {
        perror("Error: message queue");
        exit(0);
    }


    sem = sem_open("p5sem", O_CREAT, 0777, 1); // open semaphore and protect clock

    ptr = shmat(shmid, NULL, 0); // attach shared memory
}

/* ====================================================================================================
* Detach the shared memory and semaphore
==================================================================================================== */
void detachSharedMemory()
{
    shmctl(shmid_mem, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(messageQUEUE, IPC_RMID, NULL);
    sem_unlink("p5sem");
    sem_close(sem);
}

/* ====================================================================================================
* Control types of errors
==================================================================================================== */
void signalErrors(int signum)
{
    if (signum == SIGINT)
    {
        printf("\nInterupted by ctrl-c\n");
        detachSharedMemory();
        Statistics();
    }
    else
    {
        printf("\nInterupted by %d second alarm\n", timer);
        detachSharedMemory();
        Statistics();
    }
    exit(0);
}