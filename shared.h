#ifndef SHARED_H
#define SHARED_H

#define MAXPRO 18

struct time
{
    int ns; // nanoseconds
    int s;  // seconds
};

typedef struct
{
    int memoryType;
    int count;
    int write;
    float weights[32];
} resourceInfo;

struct memory
{
    int frame[256];
    int referenceBit[256];
    int dirty[256];
    int bitvector[256];
    int pagetable[MAXPRO][32];
    int pagelocation[576];
    int referenceStat;
};

typedef struct shmStruct
{
    resourceInfo resource;
    struct time time;
} sm;

#endif