#include "Thread.h"
#include "Init.h"
#include "Scheduler.h"
#include <stdio.h>
#include <stdlib.h>

BOOL isChildEnd = 0; // 0->false 1->true
Thread* joinThread = NULL;
Thread* exitThread = NULL;

int thread_create(thread_t *thread, thread_attr_t *attr, int priority,
                  void *(*start_routine)(void *), void *arg) {
    int flags = SIGCHLD | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM;
    char *pStack = malloc(STACK_SIZE);
    *thread = clone(start_routine, (char *)pStack + STACK_SIZE, flags, arg);
    kill(*thread, SIGSTOP);
    // printf("주소1 %p\n", thread);
    // printf("thread_create %d\n", *thread);

    Thread *trd = malloc(sizeof(Thread));
    trd->stackSize = STACK_SIZE;
    trd->stackAddr = pStack;
    trd->priority = priority;
    trd->pid = *thread;
    trd->status = THREAD_STATUS_READY;
    // 20200509 우선순위가 pCurrent 보다 낮으면 바로 cpu로 보내는 코드 작성.

    if (pReadyQueueEnt[priority].pTail == NULL) {
        pReadyQueueEnt[priority].pTail = trd;
        pReadyQueueEnt[priority].pHead = trd;
        trd->phNext = NULL;
        trd->phPrev = NULL;
    }

    else {
        pReadyQueueEnt[priority].pTail->phPrev = trd;
        trd->phNext = pReadyQueueEnt[priority].pTail;
        trd->phPrev = NULL;
        pReadyQueueEnt[priority].pTail = trd;
    }

    pReadyQueueEnt[priority].queueCount += 1;

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTblEnt[i].bUsed == 0) {
            pThreadTblEnt[i].bUsed = 1;
            pThreadTblEnt[i].pThread = trd;
            *thread = i;
            break;
        }
    }

    // printf("우선순위 %d 주소 %p\n", priority, trd);

    return 0;
}

int thread_suspend(thread_t tid) {
    if (pThreadTblEnt[tid].bUsed == 0 ||
        pThreadTblEnt[tid].pThread->status == THREAD_STATUS_ZOMBIE) {
        return -1;
    }
    Thread *tempThread = pThreadTblEnt[tid].pThread;
    // waiting  큐로 옮기기
    if (tempThread->status == THREAD_STATUS_READY) { //레디큐에 있을 때
        if (pReadyQueueEnt[tempThread->priority].queueCount ==
            1) { //레디큐에 하나만 있음
            pReadyQueueEnt[tempThread->priority].pTail = NULL;
            pReadyQueueEnt[tempThread->priority].pHead = NULL;
        } else { //레디큐에 여러개있음.
            if (pReadyQueueEnt[tempThread->priority].pTail == tempThread) {
                pReadyQueueEnt[tempThread->priority].pTail->phNext->phPrev =
                    NULL;
                pReadyQueueEnt[tempThread->priority].pTail =
                    pReadyQueueEnt[tempThread->priority].pTail->phNext;
            } else if (pReadyQueueEnt[tempThread->priority].pHead ==
                       tempThread) {
                pReadyQueueEnt[tempThread->priority].pHead->phPrev->phNext =
                    NULL;
                pReadyQueueEnt[tempThread->priority].pHead =
                    pReadyQueueEnt[tempThread->priority].pHead->phPrev;
            } else {
                tempThread->phPrev->phNext = tempThread->phNext;
                tempThread->phNext->phPrev = tempThread->phPrev;
            }
        }
        tempThread->phPrev = NULL;
        tempThread->phNext = NULL;
        pReadyQueueEnt[tempThread->priority].queueCount -= 1;
    }
    //웨이팅 큐에 있는건 그냥 상태만 바꿈.
    //head로 넣고 tail 로 뺌.
    if (pWaitingQueueTail == NULL) {
        pWaitingQueueTail = tempThread;
        pWaitingQueueHead = tempThread;
        // pWaitingQueueHead->phPrev = pWaitingQueueTail;
        // pWaitingQueueTail->phNext = pWaitingQueueHead;
    } else {
        pWaitingQueueHead->phPrev = tempThread;
        tempThread->phNext = pWaitingQueueHead;
        pWaitingQueueHead = tempThread;
        // pWaitingQueueTail->phNext = tempThread;
        // tempThread->phPrev = pWaitingQueueTail;
    }// waiting큐 수정

    tempThread->status = THREAD_STATUS_WAIT;

    return 0;
}

int thread_cancel(thread_t tid) {
    //종료시키는 함수.
    // printf("cancel\n");
    if (pThreadTblEnt[tid].bUsed == 0) {
        return -1;
    }
    Thread *tempThread = pThreadTblEnt[tid].pThread;
    kill(tempThread->pid, SIGKILL);
    //삭제하고
    if (tempThread->status == THREAD_STATUS_READY) { //래디큐에 있으면
        if (pReadyQueueEnt[tempThread->priority].queueCount ==
            1) { //레디큐에 하나만 있음
            pReadyQueueEnt[tempThread->priority].pTail = NULL;
            pReadyQueueEnt[tempThread->priority].pHead = NULL;
        } else { //레디큐에 여러개있음.
            if (pReadyQueueEnt[tempThread->priority].pTail == tempThread) {
                pReadyQueueEnt[tempThread->priority].pTail->phNext->phPrev =
                    NULL;
                pReadyQueueEnt[tempThread->priority].pTail =
                    pReadyQueueEnt[tempThread->priority].pTail->phNext;
            } else if (pReadyQueueEnt[tempThread->priority].pHead ==
                       tempThread) {
                pReadyQueueEnt[tempThread->priority].pHead->phPrev->phNext =
                    NULL;
                pReadyQueueEnt[tempThread->priority].pHead =
                    pReadyQueueEnt[tempThread->priority].pHead->phPrev;
            } else {
                tempThread->phPrev->phNext = tempThread->phNext;
                tempThread->phNext->phPrev = tempThread->phPrev;
            }
        }
        pReadyQueueEnt[tempThread->priority].queueCount -= 1;
    } else if (tempThread->status == THREAD_STATUS_WAIT) { //웨이트큐에 있으면
        //웨이트큐에서 빼기
        if (pWaitingQueueHead == pWaitingQueueTail) { //웨이트큐에 하나 있음
            pWaitingQueueHead = NULL;
            pWaitingQueueTail = NULL;
        } else {                                   // 두 개 이상
            if (pWaitingQueueHead == tempThread) { // 맨왼쪽
                pWaitingQueueHead->phNext->phPrev = NULL;
                pWaitingQueueHead = pWaitingQueueHead->phNext;
            } else if (pWaitingQueueTail == tempThread) { // 맨오른쪽
                pWaitingQueueTail->phPrev->phNext = NULL;
                pWaitingQueueTail = pWaitingQueueTail->phPrev;
            } else { // 사이
                tempThread->phPrev->phNext = tempThread->phNext;
                tempThread->phNext->phPrev = tempThread->phPrev;
            }
        }
    }
    //할당 없애기
    // deallocate하기.
    free(tempThread->stackAddr);
    free(tempThread);

    return 0;
    //자기 자신에 대해서 캔슬은 없다.
}

int thread_resume(thread_t tid) {
    if (pThreadTblEnt[tid].bUsed == 0 ||
        pThreadTblEnt[tid].pThread->status != THREAD_STATUS_WAIT) {
        return -1;
    }

    Thread *tempThread = pThreadTblEnt[tid].pThread;
    //웨이트큐에서 일단 빼고 뺀걸 어떻게 할지 정하자
    //웨이트큐에서 빼기
    if (pWaitingQueueHead == pWaitingQueueTail) { //웨이트큐에 하나 있음
        pWaitingQueueHead = NULL;
        pWaitingQueueTail = NULL;
    } else {                                   // 두 개 이상
        if (pWaitingQueueHead == tempThread) { // 맨왼쪽
            pWaitingQueueHead->phNext->phPrev = NULL;
            pWaitingQueueHead = pWaitingQueueHead->phNext;
        } else if (pWaitingQueueTail == tempThread) { // 맨오른쪽
            pWaitingQueueTail->phPrev->phNext = NULL;
            pWaitingQueueTail = pWaitingQueueTail->phPrev;
        } else { // 사이
            tempThread->phPrev->phNext = tempThread->phNext;
            tempThread->phNext->phPrev = tempThread->phPrev;
        }
    }
    tempThread->phNext = NULL;
    tempThread->phPrev = NULL;
    // tempThread 추출함. 이걸 레디큐에 넣을지 cpu에 넣을지 고민.
    if (tempThread->priority >= pCurrentThread->priority) { //우선순위가 낮으면(숫자가 높으면) 레디큐로
        tempThread->status = THREAD_STATUS_READY;
        if (pReadyQueueEnt[tempThread->priority].queueCount == 0) {
            pReadyQueueEnt[tempThread->priority].pHead = tempThread;
            pReadyQueueEnt[tempThread->priority].pTail = tempThread;
            tempThread->phPrev = NULL;
            tempThread->phNext = NULL;
        } else {
            tempThread->phNext = pReadyQueueEnt[tempThread->priority].pTail;
            pReadyQueueEnt[tempThread->priority].pTail->phPrev = tempThread;
            pReadyQueueEnt[tempThread->priority].pTail = tempThread;
        }
        pReadyQueueEnt[tempThread->priority].queueCount += 1;
        //레디큐로 넣기
    } else { //우선순위가 높으면(숫자가 낮으면) cpu로 보낸다.
        //컨텍스팅 스위칭을 하는데
        //기존 연결을 끊어야 하니까
        // alarm(0)하고 바로 context switching 호출.
        // 런상태로 바꿔야함.
        // 20200509 해야할 것. context switching 바꾸기
        Thread *changeThread = pCurrentThread;

        pCurrentThread->phNext = NULL;
        pCurrentThread->phPrev =NULL;
        if(pReadyQueueEnt[pCurrentThread->priority].queueCount == 0){
            pReadyQueueEnt[pCurrentThread->priority].pHead = pCurrentThread;
            pReadyQueueEnt[pCurrentThread->priority].pTail = pCurrentThread;
        }
        else{
            pReadyQueueEnt[pCurrentThread->priority].pTail->phPrev = pCurrentThread;
            pCurrentThread->phNext = pReadyQueueEnt[pCurrentThread->priority].pTail;
            pReadyQueueEnt[pCurrentThread->priority].pTail = pCurrentThread;
        }

        pReadyQueueEnt[pCurrentThread->priority].queueCount+=1;
        pCurrentThread->status = THREAD_STATUS_WAIT;
        tempThread->status = THREAD_STATUS_RUN;
        pCurrentThread = tempThread;
        __ContextSwitch(changeThread->pid, tempThread->pid);
    }
    return 0;
}

thread_t thread_self() {
    pid_t pid = getpid();

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTblEnt[i].pThread->pid == pid) {
            return i;
        }
    }
}

void joinHandler() {
    // exit의 결과로 waiting 큐에 있고
    //그리고 Context switching 해야함.
    //자식이 stop 받아도 chld 받네
    //그러니까 stop 시에 chld 시그널 받을 때 아님을 확인해야함.
    if(joinThread==NULL || joinThread->status != THREAD_STATUS_ZOMBIE){
        // printf("%d 아직 좀비아님\n", joinThread->pid);
        // signal(SIGCHLD, joinHandler);
        // pause();
        return;
    }
    // printf("자식종료\n");

    __ContextSwitch(-1, findNextPid());
}

int thread_join(thread_t tid, void **retval) {
    // printf("thread join %d\n", tid);
    Thread *tempThread = pThreadTblEnt[tid].pThread; // 자식 스레드
    // printf("조인 : %d %d %p\n", tempThread->pid, tempThread->status, tempThread->stackAddr);
    Thread *joinThread = tempThread;

    //좀비 아니면 (아직 안끝났고, 웨이팅큐에 없는 상태)
    if (tempThread->status != THREAD_STATUS_ZOMBIE) {
        thread_t currentThreadIndex = thread_self();
        if (pThreadTblEnt[currentThreadIndex].pThread->status !=
            THREAD_STATUS_WAIT) {
            pThreadTblEnt[currentThreadIndex].pThread->status =
                THREAD_STATUS_WAIT;
            if (pWaitingQueueTail == NULL) {
                pWaitingQueueTail = pThreadTblEnt[currentThreadIndex].pThread;
                pWaitingQueueHead = pThreadTblEnt[currentThreadIndex].pThread;
            } else {
                pWaitingQueueHead->phPrev = pThreadTblEnt[currentThreadIndex].pThread;
                pThreadTblEnt[currentThreadIndex].pThread->phNext = pWaitingQueueHead;
            }
            pCurrentThread = NULL;

            __ContextSwitch(-1, findNextPid());

        }
        // printf("부모 정지\n");
        signal(SIGCHLD, joinHandler);
        //자식 함수 끝날 때 까지 기다림.
        while(joinThread->status != THREAD_STATUS_ZOMBIE){
            pause();
        }
        // printf("부모 일어남\n");
    }
    // else 좀비이면 이미 끝났으면
    // printf("%d %d\n", tid, joinThread->exitCode);
    *retval = &(joinThread->exitCode);
    thread_cancel(tid);

    return 0;
    // exitcode 관련 뭐 하기
    //여기에 자식 메모리 웨이팅 큐에서 지우기.
}

int thread_exit(void *retval) {
    // retval 는 exitcode 이고 스레드 조인에 기다리는 곳에 전달해야함.
    // printf("exit\n");
    // printf("123\n");
    if (pWaitingQueueTail == NULL) {
        pWaitingQueueHead = pCurrentThread;
        pWaitingQueueTail = pCurrentThread;
    } else {
        pWaitingQueueHead->phPrev = pCurrentThread;
        pCurrentThread->phNext = pWaitingQueueHead;
        pWaitingQueueHead = pCurrentThread;
        // pWaitingQueueTail->phNext = pCurrentThread;
        // pCurrentThread->phPrev = pWaitingQueueTail;
    }

    // printf("456\n");
    pCurrentThread->exitCode = *((int*)retval);
    // printf("789\n");
    pCurrentThread->status = THREAD_STATUS_ZOMBIE;
    // printf("exitCode : %d\n", pCurrentThread->exitCode);
    pCurrentThread = NULL;
    exit(0);
    return 0;
}
