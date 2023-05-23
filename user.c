#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <time.h>
#include "shared.h"

/* ========================================User=======================================================

* Should take a command line argument (-m x) wherexspecifies which of the following memory request 
schemesis used in the child processes

* The first memory request scheme is simple. When a process needs to generate an address to request,
 it simply generatesa random value from 0 to the limit of the process memory (32k

* The second memory request scheme tries to favor certain pages over others.  You should implement
 a scheme whereeach of the 32 pages of a process’ memory space has a different weight of being selected

* The statistics of interest are:
    •  Number of memory accesses per second
    •  Number of page faults per memory access
    •  Average memory access speed
    •  Number of seg faults per memory access

==================================================================================================== */



/* ====================================================================================================
* Shared memory setups and semaphores
==================================================================================================== */

int shmid;
sm *ptr;
sem_t *sem;

int messageQUEUE;
float pageWeight;
float processWeightBound;

struct message
{
    long messageType;
    char mtext[512];
} msg;

void incClock(struct time *time, int sec, int ns);

/* ====================================================================================================
* Main function
==================================================================================================== */
int main(int argc, char *argv[])
{

    time_t t;
    time(&t);
    srand((int)time(&t) % getpid());


    if ((shmid = shmget(9784, sizeof(sm), 0600)) < 0)
    { // get the shared memoryt=
        exit(0);
    }

    if ((messageQUEUE = msgget(4020015, 0777)) == -1)
    {
        perror("Error: message queue");
        exit(0);
    }


    sem = sem_open("p5sem", 0); // open semaphore


    ptr = shmat(shmid, NULL, 0); // attach memory

    /* ====================================================================================================
    * Set time for when a process should either request or release 
    ==================================================================================================== */
    int nextMove = (rand() % 1000000 + 1);
    struct time moveTime;
    sem_wait(sem);
    moveTime.s = ptr->time.s;
    moveTime.ns = ptr->time.ns;
    sem_post(sem);

    /* ====================================================================================================
    * Set time for when the processes shoudl check ifg they need to terminate 
    ==================================================================================================== */
    int termination = (rand() % (250 * 1000000) + 1);
    struct time termCheck;
    sem_wait(sem);
    termCheck.s = ptr->time.s;
    termCheck.ns = ptr->time.ns;
    sem_post(sem);

    if (ptr->resource.memoryType == 0)
    {
        while (1)
        {
            /* ====================================================================================================
            * Request or release 
            ==================================================================================================== */
            if ((ptr->time.s > moveTime.s) || (ptr->time.s == moveTime.s && ptr->time.ns >= moveTime.ns))
            {
                /* ====================================================================================================
                * Set time for next action
                ==================================================================================================== */
                sem_wait(sem);
                moveTime.s = ptr->time.s;
                moveTime.ns = ptr->time.ns;
                sem_post(sem);
                incClock(&moveTime, 0, nextMove);

                /* ====================================================================================================
                * See if process needs to request read/write --> gen rando address 
                ==================================================================================================== */
                if (rand() % 100 < 60)
                {
                    strcpy(msg.mtext, "REQUEST");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);

                    int request = rand() % (31998 + 1) + 1;
                    sprintf(msg.mtext, "%d", request);
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
                else
                {
                    strcpy(msg.mtext, "WRITE");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                    int write = rand() % (31998 + 1) + 1;
                    sprintf(msg.mtext, "%d", write);
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
            }

            /* ====================================================================================================
            * Check for termination everytimne the memory refreshes 100 times
            ==================================================================================================== */
            if ((ptr->resource.count % 1000) == 0 && ptr->resource.count != 0)
            {
                if ((rand() % 100) <= 70)
                {
                    strcpy(msg.mtext, "TERMINATED");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
            }

            exit(0);
        }
    }
    /* ====================================================================================================
    * Second memory access method 
    ==================================================================================================== */
    if (ptr->resource.memoryType == 1)
    {
        while (1)
        {
            /* ====================================================================================================
            * Check if its time for next action and advance clock 
            ==================================================================================================== */
            if ((ptr->time.s > moveTime.s) || (ptr->time.s == moveTime.s && ptr->time.ns >= moveTime.ns))
            {
                sem_wait(sem);
                moveTime.s = ptr->time.s;
                moveTime.ns = ptr->time.ns;
                sem_post(sem);
                incClock(&moveTime, 0, nextMove);

                float processWeight; // initlize array when weight is 1/n 
                int i;

                for (i = 0; i < 31; i++)
                { // add each index of the array
                    ptr->resource.weights[0] = 1;
                    ptr->resource.weights[i + 1] = 1 + ptr->resource.weights[i + 1];
                }
                processWeightBound = ptr->resource.weights[i];

                 /* ====================================================================================================
                * Generate rand number and add it to the last value of the array
                ==================================================================================================== */
                int num = (rand() % (int)processWeightBound + 1);
                int j;

                 /* ====================================================================================================
                * Travel down the array until there is value bigger than the specified number 
                ==================================================================================================== */
                for (j = 0; j < 32; j++)
                {
                    if (ptr->resource.weights[j] > num)
                    {
                        pageWeight = ptr->resource.weights[j];
                        break;
                    }
                }

                 /* ====================================================================================================
                * Get actual memory address and see if its a read or write request
                ==================================================================================================== */
                float pageNUM = pageWeight * 24;
                float randomOffset = rand() % 1023;

                if (rand() % 100 < 60)
                {
                    strcpy(msg.mtext, "REQUEST");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                    int methodTwoRequest = pageNUM + randomOffset;
                    sprintf(msg.mtext, "%d", methodTwoRequest);
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
                else
                {
                    strcpy(msg.mtext, "WRITE");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                    int methodTwoRequest = pageNUM + randomOffset;
                    sprintf(msg.mtext, "%d", methodTwoRequest);
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
            }

            
            
            if ((ptr->resource.count % 100) == 0 && ptr->resource.count != 0)
            { // check termination 
                if ((rand() % 100) <= 70)
                {
                    strcpy(msg.mtext, "TERMINATED");
                    msg.messageType = 99;
                    msgsnd(messageQUEUE, &msg, sizeof(msg), 0);
                }
            }

            exit(0);
        }
    }
    return 0;
}

/* ====================================================================================================
* Increment clock and procection with semaphore
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