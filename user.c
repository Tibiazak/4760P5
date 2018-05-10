/*
 * Joshua Bearden
 * CS4760 Project 5
 *
 * This program is designed to be executed by OSS. It simulates a user process that will read a clock within a
 * mutually exclusive critical section (utilizing message queues), add an amount of "work done" to the clock,
 * and send a signal to the parent (OSS) when it terminates.
 *
 * This code includes an excerpt that I obtained from StackOverflow, cited in an inline comment at line 69.
 * The code obtained is simply an elegant solution to generating random numbers greater than RAND_MAX.
 *
 * This program uses one function: interrupt. Interrupt is an interrupt handler for SIGUSR1 which detaches shared memory
 * and then exits.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include "clock.c"
#include <semaphore.h>
#include <stdbool.h>

#define BILLION 1000000000
#define BOUND 2
#define UPPERBOUND 3
#define TERMINATIONCONSTANT 1
#define WORKCONSTANT 500000
#define SHAREKEY 92195
#define MSGKEY 110992
#define TABLEKEY 210995
#define SEM_NAME "/mutex-semaphore-jbwd4"
#define TERMINATE 1
#define REQUEST 2
#define RELEASE 3


int ClockID;
struct clock *Clock;
int MsgID;
int TableID;


sem_t *sem_for_mutex;

struct mesg_buf {
    long mtype;
    char mtext[100];
} message;


// Interrupt handler for SIGUSR1, detaches shared memory and exits cleanly.
static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    sem_close(sem_for_mutex);
    exit(1);
}


int max_resources(int table[19][20], int current_resources[20], int simpid)
{
    int i;
    for(i = 0; i < 20; i++)
    {
        if (table[simpid][i] != current_resources[i])
        {
            return 0;
        }
    }
    return 1;
}


int no_resources(int current_resources[20])
{
    int i;
    for(i = 0; i < 20; i++)
    {
        if(current_resources[i] > 0)
        {
            return 0;
        }
    }
    return 1;
}


int choose_resource_to_release(int current_resources[20])
{
    int test;
    while(true)
    {
        test = rand() % 20;
        if (current_resources[test] > 0)
        {
            return test;
        }
    }
}


int choose_resource_to_request(int table[19][20], int current_resources[20], int simpid)
{
    int test;
    while(true)
    {
        test = rand() % 20;
        if(current_resources[test] < table[simpid][test])
        {
            return test;
        }
    }
}


void do_work()
{
    // access the clock, increment by WORKCONSTANT
    printf("User about to do work\n");
    sem_wait(sem_for_mutex);
    if((Clock->nsec + WORKCONSTANT) >= BILLION)
    {
        Clock->sec++;
        Clock->nsec = Clock->nsec + WORKCONSTANT - BILLION;
    }
    else
    {
        Clock->nsec += WORKCONSTANT;
    }
    sem_post(sem_for_mutex);
    printf("User finished doing work.\n");
}


int main(int argc, char *argv[]) {
    signal(SIGUSR1, interrupt); // registers interrupt handler
    int i;
    int (*proc_table)[20];
    int current_resources[20];
    int simpid = atoi(argv[1]);
    int resource;

    for (i = 0; i < 20; i++)
    {
        current_resources[i] = 0;
    }

    printf("User: My simpid is %i\n", simpid);
    srand(getpid()); // seeds the random number generator

    // gets and attaches shared memory
    ClockID = shmget(SHAREKEY, sizeof(int), 0777);
    Clock = (struct clock *)shmat(ClockID, NULL, 0);

    TableID = shmget(TABLEKEY, sizeof(int[19][20]), 0777);
    proc_table = shmat(TableID, NULL, 0);
    proc_table = shmat(TableID, NULL, 0);

    // gets the message queue
    MsgID = msgget(MSGKEY, 0666);

    printf("User attempting to attach to the semaphore\n");

    if((sem_for_mutex = sem_open(SEM_NAME, 0)) == SEM_FAILED)
    {
        perror("user sem_open");
        exit(1);
    }

    printf("Got the semaphore\n");

//    message.mtype = 2;
//    sprintf(message.mtext, "%d %d %d %d", getpid(), donesec, donensec, totalwork);
//    msgsnd(MsgID, &message, sizeof(message), 0);

    printf("User: About to enter main loop\n");
    while(true)
    {
        if ((rand() % UPPERBOUND) > 0) {
            printf("User is doing something!\n");
            // we either request or release resources
            //check if resources are full
            message.mtype = simpid;
            if (max_resources(proc_table, current_resources, simpid)) {
                resource = choose_resource_to_release(current_resources);
                // release the resource
                sprintf(message.mtext, "%d %d %d", getpid(), RELEASE, resource);
                current_resources[resource]--;
            } else if (no_resources(current_resources)) {
                resource = choose_resource_to_request(proc_table, current_resources, simpid);
                // request the resource
                sprintf(message.mtext, "%d %d %d", getpid(), REQUEST, resource);
                current_resources[resource]++;
            } else {
                if ((rand() % 2) == 0) {
                    // request a resource
                    resource = choose_resource_to_request(proc_table, current_resources, simpid);
                    sprintf(message.mtext, "%d %d %d", getpid(), REQUEST, resource);
                    current_resources[resource]++;
                } else {
                    // release a resource
                    resource = choose_resource_to_request(proc_table, current_resources, simpid);
                    sprintf(message.mtext, "%d %d %d", getpid(), RELEASE, resource);
                    current_resources[resource]--;
                }
            }
            printf("User %i sending message\n", simpid);
            msgsnd(MsgID, &message, sizeof(message), 0);
            printf("User %i about to wait for a message\n", simpid);
            msgrcv(MsgID, &message, sizeof(message), simpid, 0);
            printf("Message received, continuing.\n");
            printf("User %i received message from Master intended for %li: ", simpid, message.mtype);
            printf(message.mtext);
            printf("\n");


            // at this point our request was granted, check for termination
            if ((rand() % 100) == TERMINATIONCONSTANT) {
                printf("User: Time to terminate\n");
                //send termination signal
                message.mtype = simpid;
                sprintf(message.mtext, "%d %d", getpid(), TERMINATE);
                msgsnd(MsgID, &message, sizeof(message), 0);
                msgrcv(MsgID, &message, sizeof(message), simpid, 0);
                shmdt(Clock);
                shmdt(proc_table);
                sem_close(sem_for_mutex);
                exit(0);
            }
        }
        do_work();
    }
}