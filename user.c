/*
 * Joshua Bearden
 * CS4760 Project 3
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

#define BILLION 1000000000

int ClockID;
struct clock *Clock;
int MsgID;

struct mesg_buf {
    long mtype;
    char mtext[100];
} message;


// Interrupt handler for SIGUSR1, detaches shared memory and exits cleanly.
static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}


int main(int argc, char *argv[]) {
    signal(SIGUSR1, interrupt); // registers interrupt handler
    int sharekey = atoi(argv[1]);
    int msgkey = atoi(argv[2]);
    unsigned long x;
    int totalwork;
    int workdone = 0;
    int work;
    int donesec;
    int donensec;

    srand(time(0)); // seeds the random number generator

    // gets and attaches shared memory
    ClockID = shmget(sharekey, sizeof(int), 0777);
    Clock = (struct clock *)shmat(ClockID, NULL, 0);

    // gets the message queue
    MsgID = msgget(msgkey, 0666);

    // The following 4 lines are taken from stackoverflow
    // https://stackoverflow.com/questions/19870276/generate-a-random-number-from-0-to-10000000
    // Solution to have a random number larger than MAX_RAND
    x = rand();
    x <<= 15;
    x ^= rand();
    x %= 1000001;

    // casts the random number to an int from a long
    // totalwork is the amount of work the program is going to do overall.
    totalwork = (int) x;

    // main loop: while we still have work to do, loop.
    // wait til we can get in the critical section
    // once in, generate a random amount of work done, and add it to the clock.
    // If we generate more work than the total we're supposed to do, only do up to totalwork
    // send a message to the queue allowing another process to get in.
    while(workdone < totalwork)
    {
        // get permission to enter the critical section
        msgrcv(MsgID, &message, sizeof(message), 3, 0);

        // generate a random amount of work to do this iteration and ensure its not too much work.
        work = rand();
        if((work + workdone) > totalwork)
        {
            work = totalwork - workdone;
        }

        // add the work we're doing to the clock.
        Clock->nsec += work;

        // if we have a billion nanoseconds, convert to seconds
        if(Clock->nsec >= BILLION)
        {
            Clock->sec++;
            Clock->nsec -= BILLION;
        }

        // add the work we just did to our total work done
        workdone += work;

        // save the current clock time (so we have it when we terminate)
        donensec = Clock->nsec;
        donesec = Clock->sec;

        // send a message allowing another process to enter the critical section
        message.mtype = 3;
        msgsnd(MsgID, &message, sizeof(message), 0);
    }

    // send the terminating message to OSS
    message.mtype = 2;
    sprintf(message.mtext, "%d %d %d %d", getpid(), donesec, donensec, totalwork);
    msgsnd(MsgID, &message, sizeof(message), 0);

    // detach shared memory and terminate
    shmdt(Clock);
    return 0;
}