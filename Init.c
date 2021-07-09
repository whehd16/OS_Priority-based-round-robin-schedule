#include "Init.h"
#include "Thread.h"
#include "Scheduler.h"
#include "MsgQueue.h"

void Init(void)
{
    pCurrentThread = NULL;
    pWaitingQueueHead = NULL;
    pWaitingQueueTail = NULL;
    for(int i = 0; i < MAX_READYQUEUE_NUM; i++){
        pReadyQueueEnt[i].queueCount = 0;
        pReadyQueueEnt[i].pHead = NULL;
        pReadyQueueEnt[i].pTail = NULL;
    }

    for(int i = 0; i < MAX_THREAD_NUM; i++){
        pThreadTblEnt[i].bUsed = 0;
        pThreadTblEnt[i].pThread = NULL;
    }

    for(int i = 0; i<MAX_QCB_NUM; i++){
        qcbTblEntry[i].bUsed=0;
    }
}
