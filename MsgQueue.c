#include "MsgQueue.h"
// #include "Init.h"
#include <string.h>

int findSameQCB(const char* name){
    for(int i = 0; i < MAX_QCB_NUM; i++){
        if(strcmp(qcbTblEntry[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

int findEmptyQCB(){
    for(int i = 0; i < MAX_QCB_NUM; i++){
        if(qcbTblEntry[i].bUsed == 0){
            return i;
        }
    }
    return -1;
}

pmqd_t pmq_open(const char* name, int flags, mode_t perm, pmq_attr* attr)
{
    //pmq : 메시지 큐를 생성하거나 이미 존재하는 mq 를 오픈함.
    //name mq의 이름
    //flags: O_CREAT만 사용.
    //perm : 접근 권한 명시, O_RDONLY, O_WRONLY, O_RDWR
    //attr 구조체인데, 과제에선 NULL로 지정함.
    //return 값으로 mq descriptor 리턴
    // printf("pmp_open\n");
    int id = findSameQCB(name);
    if(id == -1){
        id = findEmptyQCB();
        strcpy(qcbTblEntry[id].name, name);
        qcbTblEntry[id].mode = perm;
        qcbTblEntry[id].openCount = 0;
        Qcb* tempQcb = (Qcb*)malloc(sizeof(Qcb));
        tempQcb->msgCount = 0;
        tempQcb->pMsgHead = NULL;
        tempQcb->pMsgTail = NULL;
        tempQcb->waitThreadCount = 0;
        tempQcb->pWaitQHead = NULL;
        tempQcb->pWaitQTail = NULL;
        qcbTblEntry[id].pQcb = tempQcb;
        qcbTblEntry[id].bUsed = 1;
    }

    qcbTblEntry[id].openCount+=1;
    return id;

}

int pmq_close(pmqd_t mqd)
{
    //open 할 때 만들어진 mqd 인자로
    //성공하면 0, 실패하면 -1

    qcbTblEntry[mqd].openCount -= 1;
    if(qcbTblEntry[mqd].openCount == 0 && qcbTblEntry[mqd].pQcb->msgCount == 0){
        qcbTblEntry[mqd].bUsed = 0;
        free(qcbTblEntry[mqd].pQcb);
        qcbTblEntry[mqd].pQcb = NULL;
    }
    return 0;
}

Thread* isWaitingSender(Qcb* pQcb){
    Thread* tempThread = pQcb->pWaitQHead;
    // printf("%d\n",pQcb->waitThreadCount);
    if(pQcb->waitThreadCount==1){
        pQcb->pWaitQHead=NULL;
        pQcb->pWaitQTail=NULL;
    }
    else{
        // printf("hello\n");
        pQcb->pWaitQHead = pQcb->pWaitQHead->phPrev;
        pQcb->pWaitQHead->phNext = NULL;
    }
    pQcb->waitThreadCount-=1;

    tempThread->phPrev = NULL;
    tempThread->phNext = NULL;
    return tempThread;
}

void send2ReadyQueue(Thread* waitingThread){
    if(pReadyQueueEnt[waitingThread->priority].queueCount == 0){
        pReadyQueueEnt[waitingThread->priority].pHead = waitingThread;
        pReadyQueueEnt[waitingThread->priority].pTail = waitingThread;
    }
    else{
        pReadyQueueEnt[waitingThread->priority].pTail->phPrev = waitingThread;
        waitingThread->phNext = pReadyQueueEnt[waitingThread->priority].pTail;
        waitingThread->phPrev=NULL;
        pReadyQueueEnt[waitingThread->priority].pTail = waitingThread;
    }
    pReadyQueueEnt[waitingThread->priority].queueCount += 1;
}

void insertMessageOnQcb(Qcb* pQcb, Message* message){
    // printf("hello insert!\n");
    if(pQcb->msgCount == 0){//처음 넣는 메세지임.
        // printf("msgCount == 0 \n");
        message->pNext = NULL;
        message->pPrev = NULL;
        pQcb->pMsgHead = message;
        pQcb->pMsgTail = message;
        pQcb->msgCount +=1;
        //여기서 문제. 기다리고 있는 스레드가 있는지 확인해야함.
        if(pQcb->waitThreadCount > 0){
            Thread* waitingThread = isWaitingSender(pQcb);
            send2ReadyQueue(waitingThread);
        }
        return;
    }
    else{
        // printf("msgCount =! 0 \n");
        Message* tempMessage = pQcb->pMsgTail;
        for(int i = 0; i < pQcb->msgCount; i++){// 우선순위 높은게 오른쪽.
            if(message->priority <= tempMessage->priority){//temp 기준 왼쪽에 넣자.
                if(tempMessage == pQcb->pMsgTail){//왼쪽 끝에 넣기
                    pQcb->pMsgTail->pPrev = message;
                    message->pNext = pQcb->pMsgTail;
                    message->pPrev = NULL;
                    pQcb->pMsgTail = message;
                    pQcb->msgCount +=1;
                    return;
                }
                else{//중간에 넣기.
                    tempMessage->pPrev->pNext = message;
                    message->pPrev = tempMessage->pPrev;
                    tempMessage->pPrev = message;
                    message->pNext = tempMessage;
                    pQcb->msgCount +=1;
                    return;
                }
            }
            tempMessage=tempMessage->pNext;
        }

        message->pNext = NULL;
        message->pPrev = pQcb->pMsgHead;
        pQcb->pMsgHead->pNext = message;
        pQcb->pMsgHead = message;
        pQcb->msgCount +=1;

        return;
        //여기까지 왔으면 무조건 head 에 넣기
    }
}

int pmq_send(pmqd_t mqd, char* msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    //msg_ptr : 버퍼 메모리 할당 받고 데이터 채워서 인자로 넘길 것.
    //msg_len : 길이
    //msg_prio: 우선순위 0 이 가장 낮은 순위, 높을수록 먼저 리시버에게 전달.
    //성공 0, 실패 -1
    //non-blocking send 동작, 메세지를 큐에 넣고 리시버가 받을 때 가지 기다리지 않고 걍 나옴.

    Qcb* pQcb = qcbTblEntry[mqd].pQcb;
    Message* message = (Message*)malloc(sizeof(Message));
    strcpy(message->data, msg_ptr);
    message->size = msg_len;
    message->priority = msg_prio;
    //생성한 메시지를 QCB msgList 링크드 리스트에 넣는다.
    insertMessageOnQcb(pQcb, message);
    return 0;
}

Message* selectMessageOnQcb(Qcb* pQcb){
    Message* selectedMessage = pQcb->pMsgHead;
    if(pQcb->msgCount == 1){
        pQcb->pMsgHead = NULL;
        pQcb->pMsgTail = NULL;
    }
    else{
        pQcb->pMsgHead= pQcb->pMsgHead->pPrev;
        pQcb->pMsgHead->pNext = NULL;
    }
    pQcb->msgCount-=1;
    return selectedMessage;
}

void send2QcbWaitingtList(Qcb* pQcb){
    //pQcb 안에 있는 waiting list 로 넘기자.
    if(pQcb->waitThreadCount == 0){
        pQcb->pWaitQHead = pCurrentThread;
        pQcb->pWaitQTail = pCurrentThread;
        pCurrentThread->phNext=NULL;
        pCurrentThread->phPrev=NULL;
    }
    else{//tail에 넣자.
        pQcb->pWaitQTail->phPrev = pCurrentThread;
        pCurrentThread->phPrev = NULL;
        pCurrentThread->phNext = pQcb->pWaitQTail;
        pQcb->pWaitQTail = pCurrentThread;
    }
    pQcb->waitThreadCount+=1;
    return;
}
ssize_t pmq_receive(pmqd_t mqd, char* msg_ptr, size_t msg_len, unsigned int* msg_prio)
{
    //msf_ptr: 받아올 공간, 리시버에서 받을 공간 할당해서 길이 (msg_len), (msg_prio) 받아옴.
    //msg_prio : NULL이면 메시지 우선순위 받지 않겠다는 것을 의미함.
    //성공하면 받은 메세지의 길이를, 실패하면 -1을 반환함.
    //충분히 큰 버퍼 할당할 것.

    //msg queue가 비어있으면 이 함수를 호출한 스레드는 waiting 상태가 된다.
    //당연히 waiting queue에 들어가야한다.
    //과제2의 waiting queue가 아니라 msg queue 에 있는 thread waiting queue에 들어감(여기서 잠듦)
    //따라서 과제4에서 msg queue에 waiting queue를 만들어야 한다.

    //메시지 도착하지 않으면 wait 여기서 리턴하지말고 cpu 놀기 때문에 ready queue에 있는 thread 실행.
    //sender가 보내면 리시버는 레디상태
    // printf("pmq_receive\n");
    //일단은 메세지가 있다는 가정하에
    Qcb* pQcb = qcbTblEntry[mqd].pQcb;
    Message* message;

    while(1){
        if(pQcb->msgCount > 0){
            message = selectMessageOnQcb(pQcb);
            // strcpy(msg_ptr, message);
            strcpy(msg_ptr, message->data);
            *msg_prio = message->priority;
            int messageLength = message->size;
            free(message);
            return messageLength;
        }
        else{
            //현재 스레드를 wait 상태로 바꿈.
            pid_t currentPid = pCurrentThread->pid;
            pCurrentThread->status = THREAD_STATUS_WAIT;
            send2QcbWaitingtList(pQcb);
            pCurrentThread = NULL;
            __ContextSwitch(currentPid, findNextPid());
            // printf("풀림\n");
            //53분 10초 해야함.
        }
    }
}
