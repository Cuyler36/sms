#include <dolphin.h>
#include <dolphin/os.h>
#include <macros.h>

#include "__os.h"

#define ENQUEUE_MUTEX(mutex, queue, link)                                      \
	do {                                                                       \
		struct OSMutex* __prev = (queue)->tail;                                \
		if (__prev == NULL) {                                                  \
			(queue)->head = (mutex);                                           \
		} else {                                                               \
			__prev->link.next = (mutex);                                       \
		}                                                                      \
		(mutex)->link.prev = __prev;                                           \
		(mutex)->link.next = 0;                                                \
		(queue)->tail      = (mutex);                                          \
	} while (0);

#define DEQUEUE_MUTEX(mutex, queue, link)                                      \
	do {                                                                       \
		struct OSMutex* __next = (mutex)->link.next;                           \
		struct OSMutex* __prev = (mutex)->link.prev;                           \
		if (__next == NULL) {                                                  \
			(queue)->tail = __prev;                                            \
		} else {                                                               \
			__next->link.prev = __prev;                                        \
		}                                                                      \
		if (__prev == NULL) {                                                  \
			(queue)->head = __next;                                            \
		} else {                                                               \
			__prev->link.next = __next;                                        \
		}                                                                      \
	} while (0);

#define DEQUEUE_HEAD(mutex, queue, link)                                       \
	do {                                                                       \
		struct OSMutex* __next = mutex->link.next;                             \
		if (__next == NULL) {                                                  \
			(queue)->tail = 0;                                                 \
		} else {                                                               \
			__next->link.prev = 0;                                             \
		}                                                                      \
		(queue)->head = __next;                                                \
	} while (0);

void OSInitMutex(struct OSMutex* mutex)
{
	OSInitThreadQueue(&mutex->queue);
	mutex->thread = 0;
	mutex->count  = 0;
}

void OSLockMutex(struct OSMutex* mutex)
{
	int enabled                    = OSDisableInterrupts();
	struct OSThread* currentThread = OSGetCurrentThread();

	ASSERTMSGLINE(0x8C, currentThread,
	              "OSLockMutex(): current thread does not exist.");
	ASSERTMSGLINE(0x8E, currentThread->state == 2,
	              "OSLockMutex(): current thread is not running.");

	while (1) {
		struct OSThread* ownerThread = mutex->thread;
		if (ownerThread == 0) {
			mutex->thread = currentThread;
			mutex->count++;
			ENQUEUE_MUTEX(mutex, &currentThread->queueMutex, link);
			break;
		} else if (ownerThread == currentThread) {
			mutex->count++;
			break;
		} else {
			currentThread->mutex = mutex;
			__OSPromoteThread(mutex->thread, currentThread->priority);
			OSSleepThread(&mutex->queue);
			currentThread->mutex = NULL;
		}
	}
	OSRestoreInterrupts(enabled);
}

void OSUnlockMutex(struct OSMutex* mutex)
{
	int enabled                    = OSDisableInterrupts();
	struct OSThread* currentThread = OSGetCurrentThread();

	ASSERTMSGLINE(0xBD, currentThread,
	              "OSUnlockMutex(): current thread does not exist.");
	ASSERTMSGLINE(0xBF, currentThread->state == 2,
	              "OSUnlockMutex(): current thread is not running.");
	ASSERTMSG2LINE(
	    0xC2, mutex->thread == currentThread,
	    "OSUnlockMutex(): current thread %p is not the owner of mutex %p.",
	    currentThread, mutex);

	if (mutex->thread == currentThread) {
		if (!--mutex->count) {
			DEQUEUE_MUTEX(mutex, &currentThread->queueMutex, link);
			mutex->thread = 0;

			if (currentThread->priority < currentThread->base) {
				currentThread->priority
				    = __OSGetEffectivePriority(currentThread);
			}
			OSWakeupThread(&mutex->queue);
		}
	}
	OSRestoreInterrupts(enabled);
}

void __OSUnlockAllMutex(struct OSThread* thread)
{
	struct OSMutex* mutex;

	while (thread->queueMutex.head) {
		mutex = thread->queueMutex.head;
		DEQUEUE_HEAD(mutex, &thread->queueMutex, link);
		ASSERTLINE(0xE5, mutex->thread == thread);
		mutex->count  = 0;
		mutex->thread = 0;
		OSWakeupThread(&mutex->queue);
	}
}

int OSTryLockMutex(struct OSMutex* mutex)
{
	int enabled                    = OSDisableInterrupts();
	struct OSThread* currentThread = OSGetCurrentThread();
	int locked;

	ASSERTMSGLINE(0xFF, currentThread,
	              "OSTryLockMutex(): current thread does not exist.");
	ASSERTMSGLINE(0x101, currentThread->state == 2,
	              "OSTryLockMutex(): current thread is not running.");

	if (!mutex->thread) {
		mutex->thread = currentThread;
		mutex->count++;
		ENQUEUE_MUTEX(mutex, &currentThread->queueMutex, link);
		locked = 1;
	} else if (mutex->thread == currentThread) {
		mutex->count++;
		locked = 1;
	} else {
		locked = 0;
	}
	OSRestoreInterrupts(enabled);
	return locked;
}

void OSInitCond(struct OSCond* cond) { OSInitThreadQueue(&cond->queue); }

void OSWaitCond(struct OSCond* cond, struct OSMutex* mutex)
{
	int enabled                    = OSDisableInterrupts();
	struct OSThread* currentThread = OSGetCurrentThread();

	ASSERTMSGLINE(0x139, currentThread,
	              "OSWaitCond(): current thread does not exist.");
	ASSERTMSGLINE(0x13B, currentThread->state == 2,
	              "OSWaitCond(): current thread is not running.");
	ASSERTMSG2LINE(
	    0x13E, mutex->thread == currentThread,
	    "OSWaitCond(): current thread %p is not the owner of mutex %p.",
	    currentThread, mutex);

	if (mutex->thread == currentThread) {
		long count   = mutex->count;
		mutex->count = 0;
		DEQUEUE_MUTEX(mutex, &currentThread->queueMutex, link);
		mutex->thread = 0;
		if (currentThread->priority < currentThread->base) {
			currentThread->priority = __OSGetEffectivePriority(currentThread);
		}
		OSDisableScheduler();
		OSWakeupThread(&mutex->queue);
		OSEnableScheduler();
		OSSleepThread(&cond->queue);
		OSLockMutex(mutex);
		mutex->count = count;
	}
	OSRestoreInterrupts(enabled);
}

void OSSignalCond(struct OSCond* cond) { OSWakeupThread(&cond->queue); }
