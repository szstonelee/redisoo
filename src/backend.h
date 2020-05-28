#ifndef __BACKEND_H
#define __BACKEND_H

#if defined(__APPLE__)
    #include <os/lock.h>
    static os_unfair_lock spinLock;
    // static os_unfair_lock spinLock = OS_UNFAIR_LOCK_INIT;
    #define rocklock() os_unfair_lock_lock(&spinLock)
    #define rockunlock() os_unfair_lock_unlock(&spinLock)

#else
    #include <pthread.h>
    static pthread_spinlock_t spinLock;
    #define rocklock() pthread_spin_lock(&spinLock)
    #define rockunlock() pthread_spin_unlock(&spinLock)
#endif

/* API */
void initSpinLock();
void initBackendZeroJobs();
void initBackendWorkingThreads();
void checkNeedToBackendState(client *c);
void releaseWhenFreeClient(client *c);

#endif
