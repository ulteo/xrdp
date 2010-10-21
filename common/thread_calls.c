/*
   Copyright (c) 2004-2009 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   thread calls

*/

#include "thread_calls.h"


int APP_CC
barrier_init(barrier_t *barrier,int needed)
{
    barrier->needed = needed;
    barrier->called = 0;
    pthread_mutex_init(&barrier->mutex,NULL);
    pthread_cond_init(&barrier->cond,NULL);
    return 0;
}

int APP_CC
barrier_destroy(barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}

int APP_CC
barrier_wait(barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    barrier->called++;
    if (barrier->called == barrier->needed) {
        barrier->called = 0;
        pthread_cond_broadcast(&barrier->cond);
    } else {
        pthread_cond_wait(&barrier->cond,&barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

/*****************************************************************************/
/* returns error */
#if defined(_WIN32)
int APP_CC
tc_thread_create(unsigned long (__stdcall * start_routine)(void*), void* arg)
{
  DWORD thread_id;
  HANDLE thread;
  int rv;

  /* CreateThread returns handle or zero on error */
  thread = CreateThread(0, 0, start_routine, arg, 0, &thread_id);
  rv = !thread;
  CloseHandle(thread);
  return rv;
}
#else
int APP_CC
tc_thread_create(void* (* start_routine)(void*), void* arg)
{
  pthread_t thread;
  int rv;

  /* pthread_create returns error */
  rv = pthread_create(&thread, 0, start_routine, arg);
  pthread_detach(thread);
  return rv;
}
#endif

/*****************************************************************************/
tbus APP_CC
tc_get_threadid(void)
{
#if defined(_WIN32)
  return (tbus)GetCurrentThreadId();
#else
  return (tbus)pthread_self();
#endif
}

/*****************************************************************************/
/* returns boolean */
int APP_CC
tc_threadid_equal(tbus tid1, tbus tid2)
{
#if defined(_WIN32)
  return tid1 == tid2;
#else
  return pthread_equal((pthread_t)tid1, (pthread_t)tid2);
#endif
}

/*****************************************************************************/
tbus APP_CC
tc_mutex_create(void)
{
#if defined(_WIN32)
  return (tbus)CreateMutex(0, 0, 0);
#else
  pthread_mutex_t* lmutex;

  lmutex = (pthread_mutex_t*)g_malloc(sizeof(pthread_mutex_t), 0);
  pthread_mutex_init(lmutex, 0);
  return (tbus)lmutex;
#endif
}

/*****************************************************************************/
void APP_CC
tc_mutex_delete(tbus mutex)
{
#if defined(_WIN32)
  CloseHandle((HANDLE)mutex);
#else
  pthread_mutex_t* lmutex;

  lmutex = (pthread_mutex_t*)mutex;
  pthread_mutex_destroy(lmutex);
  g_free(lmutex);
#endif
}

/*****************************************************************************/
int APP_CC
tc_mutex_lock(tbus mutex)
{
#if defined(_WIN32)
  WaitForSingleObject((HANDLE)mutex, INFINITE);
  return 0;
#else
  pthread_mutex_lock((pthread_mutex_t*)mutex);
  return 0;
#endif
}

/*****************************************************************************/
int APP_CC
tc_mutex_unlock(tbus mutex)
{
#if defined(_WIN32)
  ReleaseMutex((HANDLE)mutex);
  return 0;
#else
  pthread_mutex_unlock((pthread_mutex_t*)mutex);
  return 0;
#endif
}

/*****************************************************************************/
tbus APP_CC
tc_sem_create(int init_count)
{
#if defined(_WIN32)
  HANDLE sem;

  sem = CreateSemaphore(0, init_count, init_count + 10, 0);
  return (tbus)sem;
#else
  sem_t* sem;

  sem = g_malloc(sizeof(sem_t), 0);
  sem_init(sem, 0, init_count);
  return (tbus)sem;
#endif
}

/*****************************************************************************/
void APP_CC
tc_sem_delete(tbus sem)
{
#if defined(_WIN32)
  CloseHandle((HANDLE)sem);
#else
  sem_t* lsem;

  lsem = (sem_t*)sem;
  sem_destroy(lsem);
  g_free(lsem);
#endif
}

/*****************************************************************************/
int APP_CC
tc_sem_dec(tbus sem)
{
#if defined(_WIN32)
  WaitForSingleObject((HANDLE)sem, INFINITE);
  return 0;
#else
  sem_wait((sem_t*)sem);
  return 0;
#endif
}

/*****************************************************************************/
int APP_CC
tc_sem_inc(tbus sem)
{
#if defined(_WIN32)
  ReleaseSemaphore((HANDLE)sem, 1, 0);
  return 0;
#else
  sem_post((sem_t*)sem);
  return 0;
#endif
}
