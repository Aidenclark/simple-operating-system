#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include "shared.h"

struct waitQueue {
    int queue[18]; 
};

struct waitQueue wPtr;

/* ====================================================================================================
* Initilise the queue
==================================================================================================== */
void create() 
{
    	int i;
    	for(i=0; i<18; i++) 
	{
        	wPtr.queue[i] = -1;
    	}
}

/* ====================================================================================================
* Get next process from the queue
==================================================================================================== */
int getProcess() 
{
    	if (wPtr.queue[0] == -1) 
	{
        	return -1;
    	}
    	else 
	{
		return wPtr.queue[0];
	}
}

/* ====================================================================================================
* If proc is blocked then insert into the queue
==================================================================================================== */
int addProcess (int waitPid) 
{
    	int i;
    	for (i=0; i<18; i++) 
	{
        	if (wPtr.queue[i] == -1) 
		{
            		wPtr.queue[i] = waitPid;
            		return 1;
        	}
    	}
    	return -1;
}

/* ====================================================================================================
* If request is granted then remove it from the queue
==================================================================================================== */
int removeProcess(int pNum) 
{
    	int i;
    	for (i=0; i<18; i++) 
	{
        	if (wPtr.queue[i] == pNum) 
		{ 
            		while(i+1 < 18) 
			{
                		wPtr.queue[i] = wPtr.queue[i+1];
                		i++;
            		}
            		wPtr.queue[17] = -1;
            		return 1;
        	}
    	}
    	return -1;
}

#endif