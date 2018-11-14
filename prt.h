/* prt.h - portability definitions.
 *
 * See https://github.com/UltraMessaging/lbmct
 *
 * Copyright (c) 2005-2018 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 *
 * This source code example is provided by Informatica for educational
 * and evaluation purposes only.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 * OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE LIABLE TO  * LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR INDIRECT
 * DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE TRANSACTIONS
 * CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF THE
 * LIKELIHOOD OF SUCH DAMAGES.
 */

#ifndef PRT_H
#define PRT_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Macro to approximate the basename() function. */
#define BASENAME(_p) ((strrchr(_p, '/') == NULL) ? (_p) : (strrchr(_p, '/')+1))

/* Macros to standardize error handling. */

/* IMMEDIATE FATAL ERRORS, do not return bad status, call abort().  DO NOT
 * USE THIS FOR VERY MANY THINGS!
 */
#define ENULL(_p) do {\
  if ((_p) == NULL) {\
    fprintf(stderr, "%s:%d, NULL pointer error: '%s'\n",\
      BASENAME(__FILE__), __LINE__, #_p);\
    fflush(stderr);\
    abort();\
  } \
} while (0)

#define ASSRT(cond_expr) do {\
  if (!(cond_expr)) {\
    fprintf(stderr, "ASSRT failed at %s:%d (%s)\n", BASENAME(__FILE__), __LINE__, #cond_expr);\
    fflush(stderr);\
    abort();\
  }\
} while (0)


/* Non-fatal errors, return bad status or log an error. */

#ifdef DEBUG
#define BREAKPOINT(_m) do {\
  if ((_m) != NULL)\
    printf("%s\n", (_m)?(char *)(_m):"");\
  fflush(stdout);fflush(stderr);\
  raise(5); /* invoke debugger */\
} while (0)
#else
#define BREAKPOINT(_m) do {\
  ;\
} while (0)
#endif

/* Use strdup() so that caller can pass in lbm_errmsg() as the message. */
#define E(_m) do {\
  char *_dup_m = strdup(_m);  ENULL(_dup_m);\
  lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",\
      BASENAME(__FILE__), __LINE__, _dup_m);\
  free(_dup_m);\
  BREAKPOINT(lbm_errmsg());\
} while (0)

/* Use strdup() so that caller can pass in lbm_errmsg() as the message. */
#define E_RTN(_m, _r) do {\
  char *_dup_m = strdup(_m);  ENULL(_dup_m);\
  lbm_seterrf(LBM_EINVAL, "Error at %s:%d, '%s'",\
      BASENAME(__FILE__), __LINE__, _dup_m);\
  free(_dup_m);\
  BREAKPOINT(lbm_errmsg());\
  return(_r);\
} while (0)

/* Use strdup() so that caller can pass in lbm_errmsg() as the message. */
#define EL(_m) do {\
  char *_dup_m = strdup(_m);  ENULL(_dup_m);\
  lbm_logf(LBM_LOG_ERR, "Error at %s:%d, '%s'\n",\
      BASENAME(__FILE__), __LINE__, _dup_m);\
  free(_dup_m);\
  BREAKPOINT(NULL);\
} while (0)

/* Use strdup() so that caller can pass in lbm_errmsg() as the message. */
#define EL_RTN(_m, _r) do {\
  char *_dup_m = strdup(_m);  ENULL(_dup_m);\
  lbm_logf(LBM_LOG_ERR, "Error at %s:%d, '%s'\n",\
      BASENAME(__FILE__), __LINE__, _dup_m);\
  free(_dup_m);\
  BREAKPOINT(NULL);\
  return(_r);\
} while (0)


/* Macro to make it easier to use sscanf().  See:
 *   https://stackoverflow.com/questions/25410690
 */
#define STRDEF2(_s) #_s
#define STRDEF(_s) STRDEF2(_s)


/* Safer malloc. Pass the pointer variable to receive the malloced
 * segment, the type that it points to, and the number of copies.
 */
#define PRT_MALLOC_N(_v, _t, _n) do {\
  _v = (_t *)malloc((_n)*sizeof(_t));\
  if (_v == NULL) abort();\
} while (0)

/* Safer malloc and memset to a pattern. */
#define PRT_MALLOC_SET_N(_v, _t, _p, _n) do {\
  _v = (_t *)malloc((_n)*sizeof(_t));\
  if (_v == NULL) abort();\
  memset(_v, _p, (_n)*sizeof(_t));\
} while (0)


/* See http://blog.geeky-boy.com/2014/06/clangllvm-optimize-o3-understands-free.html
 *  for why the PRT_VOL32 macro is needed. 
 */
#define PRT_VOL32(x) (*(volatile lbm_uint32_t *)&(x))


#if defined(_WIN32)

#define snprintf _snprintf

#define SLEEP_SEC(x) Sleep((x)*1000)
#define SLEEP_MSEC(x) Sleep(x)

#define MSEC_CLOCK(_v) do { \
  _v = GetTickCount64();\
} while (0)

typedef CRITICAL_SECTION prt_mutex_t;
#define PRT_MUTEX_INIT(_m) InitializeCriticalSection(&(_m))
#define PRT_MUTEX_INIT_RECURSIVE(_m) InitializeCriticalSection(&(_m))
#define PRT_MUTEX_LOCK(_m) EnterCriticalSection(&(_m))
#define PRT_MUTEX_TRYLOCK(_m) (TryEnterCriticalSection(&((_m)))==0)
#define PRT_MUTEX_UNLOCK(_m) LeaveCriticalSection(&(_m))
#define PRT_MUTEX_DELETE(_m) DeleteCriticalSection(&(_m))

typedef HANDLE prt_sem_t;
#define PRT_SEM_INIT(_s, _i) _s = CreateSemaphore(NULL, _i, INT_MAX, NULL)
#define PRT_SEM_DELETE(_s) CloseHandle(_s)
#define PRT_SEM_POST(_s) ReleaseSemaphore(&s, 1, NULL)
#define PRT_SEM_WAIT(_s) WaitForSingleObject(_s, INFINITE)

typedef HANDLE prt_thread_t;
#define PRT_THREAD_ENTRYPOINT DWORD WINAPI

#define PRT_THREAD_CREATE(_stat, _tid, _tstrt, _targ) do {\
  DWORD _ignore, _err;\
  _tid = CreateThread(NULL, 0, _tstrt, _targ, 0, &_ignore);\
  _err = GetLastError();\
  if (_tid == NULL) {\
    _stat = LBM_FAILURE;\
    lbm_seterrf(LBM_EINVAL, "Windows returned code %d", _err);\
  } else {\
    _stat = LBM_OK;\
  }\
} while (0)

#define PRT_THREAD_EXIT do { ExitThread(0); } while (0)
#define PRT_THREAD_JOIN(_tid) WaitForSingleObject(_tid, INFINITE)


#else  /******************************** Unix ********************************/

#include <unistd.h>
#include <pthread.h>

#define SLEEP_SEC(x) sleep(x)
#define SLEEP_MSEC(x) do {\
  if ((x) >= 1000){\
    sleep((x) / 1000);\
    usleep((x) % 1000 * 1000);\
  } else {\
    usleep((x)*1000);\
  }\
} while (0)

#define MSEC_CLOCK(_v) do { \
  struct timespec _ts;\
  unsigned long long _ms;\
  clock_gettime(CLOCK_MONOTONIC, &_ts);\
  _ms = _ts.tv_sec*1000 + _ts.tv_nsec/1000000;\
  _v = _ms;\
} while (0)

typedef pthread_mutex_t prt_mutex_t;
#define PRT_MUTEX_INIT_RECURSIVE(_m) \
        do { \
		  pthread_mutexattr_t errchk_attr; \
		  pthread_mutexattr_init(&errchk_attr); \
		  pthread_mutexattr_settype(&errchk_attr,PTHREAD_MUTEX_RECURSIVE); \
		  pthread_mutex_init(&(_m),&errchk_attr); \
		  pthread_mutexattr_destroy(&errchk_attr); \
        } while(0)
#define PRT_MUTEX_INIT(_m) pthread_mutex_init(&(_m),NULL)
#define PRT_MUTEX_LOCK(_m) pthread_mutex_lock(&(_m))
#define PRT_MUTEX_TRYLOCK(_m) pthread_mutex_trylock(&((_m)))
#define PRT_MUTEX_UNLOCK(_m) pthread_mutex_unlock(&(_m))
#define PRT_MUTEX_DELETE(_m) pthread_mutex_destroy(&(_m))

/* Unix semaphores are different between Apple and posix. */
#ifdef __APPLE__
#include <dispatch/dispatch.h>
typedef dispatch_semaphore_t prt_sem_t;
#define PRT_SEM_INIT(_s, _i) _s = dispatch_semaphore_create(_i)
#define PRT_SEM_DELETE(_s) dispatch_release(_s)
#define PRT_SEM_POST(_s) dispatch_semaphore_signal(_s)
#define PRT_SEM_WAIT(_s) dispatch_semaphore_wait(_s, DISPATCH_TIME_FOREVER)

#else /* Non-apple Unix */
#include <semaphore.h>
typedef sem_t prt_sem_t;
#define PRT_SEM_INIT(_s, _i) sem_init(&(_s), 0, _i)
#define PRT_SEM_DELETE(_s) sem_destroy(&(_s))
#define PRT_SEM_POST(_s) sem_post(&(_s))
#define PRT_SEM_WAIT(_s) sem_wait(&(_s))

#endif

typedef pthread_t prt_thread_t;
#define PRT_THREAD_ENTRYPOINT void *

#define PRT_THREAD_CREATE(_stat, _tid, _tstrt, _targ) do {\
  int _err;\
  _err = pthread_create(&_tid, NULL, _tstrt, _targ);\
  if (_err != 0) {\
    _stat = LBM_FAILURE;\
    lbm_seterrf(LBM_EINVAL, "%s", strerror(_err));\
  } else {\
    _stat = LBM_OK;\
  }\
} while (0)

#define PRT_THREAD_EXIT do { pthread_exit(NULL); } while (0)
#define PRT_THREAD_JOIN(_tid) pthread_join(_tid, NULL)

#endif /* else unix */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* PRT_H */
