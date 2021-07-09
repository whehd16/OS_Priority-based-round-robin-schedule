#include "Init.h"
#include "Thread.h"
#include "Scheduler.h"

BOOL isReadyQueueEmpty() {
    for (int i = 0; i < MAX_READYQUEUE_NUM; i++) {
        if (pReadyQueueEnt[i].queueCount > 0) {
            return 0;
        }
    }
    return 1;
}

void sighandler() {
    // printf("sighandler 알람 \n");
    // showTable();
    if (isReadyQueueEmpty() == 0) { //레디큐가 비어있지 않고
        if (pCurrentThread ==
            NULL) { //현재 돌고 있는 스레드가 없으면 초기 할당하는 것임
            __ContextSwitch(-1, findNextPid());
        } else { //돌고 있는 스레드가 있음.
            pid_t tempPid = pCurrentThread->pid;
            __ContextSwitch(tempPid, findNextPid());
        }
    }
    alarm(TIMESLICE);
}

int RunScheduler( void )
{
    // printf("runScheduler\n");
    signal(SIGALRM, sighandler);
    sighandler();
    while (1) {

    }
}


void __ContextSwitch(int curpid, int newpid)
{
    kill(newpid, SIGCONT);
    if(curpid != -1){
        kill(curpid, SIGSTOP);
    }
    // printf("switch %d %d\n", curpid, newpid);
}

pid_t findNextPid() {
    // printf("findNextPid\n");
    if (pCurrentThread == NULL) { //현재 돌고 있는 스레드가 없음. 초기할당
        for (int i = 0; i < MAX_READYQUEUE_NUM; i++) {
            if (pReadyQueueEnt[i].queueCount >
                0) { //처음 발견했으면 레디큐에서 빼고 cpu로 넘길 pid return
                Thread *tempThread = pReadyQueueEnt[i].pHead;
                // printf("%p\n", tempThread);
                // printf("발견한 pid : %d %d\n", i, tempThread->pid);

                if (pReadyQueueEnt[i].queueCount == 1) {
                    pReadyQueueEnt[i].pHead = NULL;
                    pReadyQueueEnt[i].pTail = NULL;
                } else {
                    pReadyQueueEnt[i].pHead->phPrev->phNext = NULL;
                    pReadyQueueEnt[i].pHead = pReadyQueueEnt[i].pHead->phPrev;
                }

                tempThread->phPrev = NULL;
                tempThread->phNext = NULL;

                pReadyQueueEnt[i].queueCount -= 1;
                pCurrentThread = tempThread;
                pCurrentThread->status =THREAD_STATUS_RUN;
                // printf("발견 %d번째\n",i);
                return tempThread->pid; //초기할당하는 스레드
            }
        }
    }

    else { //현재 돌고 있는 스레드가 있고. 타임 슬라이스가 다 됨. 무조건 리턴 값
           //있음.
        if(pCurrentThread->status == THREAD_STATUS_WAIT){
            pCurrentThread = NULL;
            return findNextPid();
        }

        for (int i = 0; i < MAX_READYQUEUE_NUM; i++) { //레디큐를 들여다 보는데
            if (i < pCurrentThread->priority) { //우선순위가 높은데
                if (pReadyQueueEnt[i].queueCount > 0) { //바꿀게 있으면
                    Thread *tempThread = pReadyQueueEnt[i].pHead;

                    //레디큐에서 빼는 과정
                    if (pReadyQueueEnt[i].queueCount == 1) {
                        pReadyQueueEnt[i].pHead = NULL;
                        pReadyQueueEnt[i].pTail = NULL;
                    } else {
                        pReadyQueueEnt[i].pHead->phPrev->phNext = NULL;
                        pReadyQueueEnt[i].pHead =
                            pReadyQueueEnt[i].pHead->phPrev;
                    }
                    tempThread->phPrev = NULL;
                    tempThread->phNext = NULL;
                    pReadyQueueEnt[i].queueCount -= 1;

                    // cpu에서 레디큐로 넣는 과정
                    if (pReadyQueueEnt[pCurrentThread->priority].queueCount ==
                        0) {
                        pReadyQueueEnt[pCurrentThread->priority].pHead =
                            pCurrentThread;
                        pReadyQueueEnt[pCurrentThread->priority].pTail =
                            pCurrentThread;
                        pCurrentThread->phPrev = NULL;
                        pCurrentThread->phNext = NULL;
                    } else {
                        pReadyQueueEnt[pCurrentThread->priority].pTail->phPrev =
                            pCurrentThread;
                        pCurrentThread->phNext =
                            pReadyQueueEnt[pCurrentThread->priority].pTail;
                        pCurrentThread->phPrev = NULL;
                        pReadyQueueEnt[pCurrentThread->priority].pTail =
                            pCurrentThread;
                    }
                    pReadyQueueEnt[pCurrentThread->priority].queueCount += 1;

                    pCurrentThread =
                        tempThread; // 여기서 context switch(pCurrentThread,
                                    // 중복되는지 확인해야함)
                    return pCurrentThread->pid;
                }
            }

            else if (i == pCurrentThread->priority) { //우선순위 같음
                if (pReadyQueueEnt[i].queueCount ==
                    0) { // cpu 돌고 있는게 전부라면
                    return pCurrentThread->pid;
                }

                else { //같은 우선순위 레디큐에 1개 이상 있다면
                    if (pReadyQueueEnt[i].queueCount == 1) { // 1개라면
                        //기존 cpu에서 돌아가는거 레디큐에 넣고
                        Thread *tempThread = pReadyQueueEnt[i].pHead;
                        pReadyQueueEnt[i].pHead = pCurrentThread;
                        pReadyQueueEnt[i].pTail = pCurrentThread;
                        pCurrentThread->phPrev = NULL;
                        pCurrentThread->phNext = NULL;
                        //레디큐에서 빼는 과정
                        pCurrentThread = tempThread;
                        pCurrentThread->phPrev = NULL;
                        pCurrentThread->phNext = NULL;
                    }

                    else { //같은 우선순위 레디큐에 2개 이상 있다면
                        //먼저 레디큐 tail에 cpu에 있는 스레드 넣기
                        pReadyQueueEnt[i].pTail->phPrev = pCurrentThread;
                        pCurrentThread->phNext = pReadyQueueEnt[i].pTail;
                        pCurrentThread->phPrev = NULL;
                        pReadyQueueEnt[i].pTail = pCurrentThread;
                        //그 다음 head 에 있는 걸 cpu로 보내기
                        Thread *tempThread = pReadyQueueEnt[i].pHead;
                        pReadyQueueEnt[i].pHead->phPrev->phNext = NULL;
                        pReadyQueueEnt[i].pHead = pReadyQueueEnt[i].pHead->phPrev;

                        tempThread->phNext = NULL;
                        tempThread->phPrev = NULL;

                        pCurrentThread->status = THREAD_STATUS_READY;
                        tempThread->status = THREAD_STATUS_RUN;
                        // pReadyQueueEnt[i].pHead->phNext = NULL;
                        // pReadyQueueEnt[i].pHead->phPrev = NULL;

                        pCurrentThread = tempThread;
                    }
                    return pCurrentThread->pid;
                }
            }
        }
    }
    //끝까지 왔다는건 레디큐에 없다는 거임.
}
