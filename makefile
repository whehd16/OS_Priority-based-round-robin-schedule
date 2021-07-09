hw4.out : main.o Init.o Thread.o Scheduler.o MsgQueue.o TestCase1.o TestCase2.o TestCase3.o TestCase4.o
		gcc -o hw4.out main.o Init.o Thread.o Scheduler.o MsgQueue.o TestCase1.o TestCase2.o TestCase3.o TestCase4.o
main.o: Init.h Thread.h Scheduler.h MsgQueue.h TestCase1.h TestCase2.h TestCase3.h TestCase4.h main.c
		gcc -c -o main.o main.c
Init.o: Init.h Thread.h Scheduler.h Init.c
		gcc -c -o Init.o Init.c
MsgQueue.o : MsgQueue.h Thread.h Scheduler.h MsgQueue.c
		gcc -c -o MsgQueue.o MsgQueue.c
Thread.o: Thread.h Init.h Scheduler.h Thread.c
		gcc -c -o Thread.o Thread.c
TestCase1.o: TestCase1.h TestCase1.c
		gcc -c -o TestCase1.o TestCase1.c
TestCase2.o: TestCase2.h TestCase2.c
		gcc -c -o TestCase2.o TestCase2.c
TestCase3.o: TestCase3.h TestCase3.c
		gcc -c -o TestCase3.o TestCase3.c
TestCase4.o: TestCase4.h TestCase4.c
		gcc -c -o TestCase4.o TestCase4.c

clean:
	rm -f *.o
