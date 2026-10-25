/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022      GrapheneCt
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/pthr.h"

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>

#include "utils/utils.h"
#include "utils/logger.h"

#define BIONIC_PTHREAD_COND_INITIALIZER              0
#define BIONIC_PTHREAD_MUTEX_INITIALIZER             0
#define BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER   0x4000
#define BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER  0x8000

enum {
    BIONIC_PTHREAD_MUTEX_NORMAL = 0,
    BIONIC_PTHREAD_MUTEX_RECURSIVE = 1,
    BIONIC_PTHREAD_MUTEX_ERRORCHECK = 2,

    BIONIC_PTHREAD_MUTEX_ERRORCHECK_NP = BIONIC_PTHREAD_MUTEX_ERRORCHECK,
    BIONIC_PTHREAD_MUTEX_RECURSIVE_NP  = BIONIC_PTHREAD_MUTEX_RECURSIVE,

    BIONIC_PTHREAD_MUTEX_DEFAULT = BIONIC_PTHREAD_MUTEX_NORMAL
};

#define PTHR_INLINE static inline __attribute__((always_inline))

/* A bionic mutex/cond is one word: an Android static initializer value, or
 * the pointer to the real object once initialized. Lazy initialization
 * publishes the pointer with a compare-and-swap so racing first lockers
 * agree on one object; explicit init overwrites whatever the word holds. */
PTHR_INLINE int _is_mutex_initializer(uintptr_t v) {
    return v == BIONIC_PTHREAD_MUTEX_INITIALIZER || v == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER ||
           v == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
}

// null check for `attr` must be performed before this
PTHR_INLINE int _attr_t_static_init(pthread_attr_t_bionic * attr) {
    if (attr->magic != 0x42424242) {
        attr->magic = 0x42424242;
        attr->real_ptr = malloc(sizeof(pthread_attr_t));
        return pthread_attr_init(attr->real_ptr);
    }
    return 0;
}

// null check for `mutex` param must be performed before this, `attr` is fine as null
static int _mutex_t_init(pthread_mutex_t_bionic * mutex, const pthread_mutexattr_t * attr, int explicit_init) {
    uintptr_t *word = (uintptr_t *) &mutex->real_ptr;
    uintptr_t value = __atomic_load_n(word, __ATOMIC_ACQUIRE);
    if (!explicit_init && !_is_mutex_initializer(value))
        return 0;

    int kind = PTHREAD_MUTEX_NORMAL;
    if (attr) {
        pthread_mutexattr_gettype((pthread_mutexattr_t *) attr, &kind);
    } else if (!explicit_init) {
        if (value == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_RECURSIVE;
        else if (value == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_ERRORCHECK;
    }

    pthread_mutex_t *real = malloc(sizeof(pthread_mutex_t));
    if (!real) return ENOMEM;
    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_settype(&mutattr, kind);
    int ret = pthread_mutex_init(real, &mutattr);
    pthread_mutexattr_destroy(&mutattr);
    if (ret != 0) {
        l_error("mutex initialization for %p has failed", mutex);
        free(real);
        return ret;
    }

    if (explicit_init) {
        __atomic_store_n(word, (uintptr_t) real, __ATOMIC_RELEASE);
    } else if (!__atomic_compare_exchange_n(word, &value, (uintptr_t) real, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        pthread_mutex_destroy(real);
        free(real);
    }
    return 0;
}

// null check for `cond` param must be performed before this, `attr` is fine as null
static int _cond_t_init(pthread_cond_t_bionic * cond, const pthread_condattr_t * attr, int explicit_init) {
    uintptr_t *word = (uintptr_t *) &cond->real_ptr;
    uintptr_t value = __atomic_load_n(word, __ATOMIC_ACQUIRE);
    if (!explicit_init && value != BIONIC_PTHREAD_COND_INITIALIZER)
        return 0;

    pthread_cond_t *real = malloc(sizeof(pthread_cond_t));
    if (!real) return ENOMEM;
    int ret = pthread_cond_init(real, attr);
    if (ret != 0) {
        l_error("cond initialization for %p has failed", cond);
        free(real);
        return ret;
    }

    if (explicit_init) {
        __atomic_store_n(word, (uintptr_t) real, __ATOMIC_RELEASE);
    } else if (!__atomic_compare_exchange_n(word, &value, (uintptr_t) real, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        pthread_cond_destroy(real);
        free(real);
    }
    return 0;
}

PTHR_INLINE pthread_mutex_t *_mutex_real(pthread_mutex_t_bionic *m) {
    return (pthread_mutex_t *) __atomic_load_n((uintptr_t *) &m->real_ptr, __ATOMIC_ACQUIRE);
}
PTHR_INLINE pthread_cond_t *_cond_real(pthread_cond_t_bionic *c) {
    return (pthread_cond_t *) __atomic_load_n((uintptr_t *) &c->real_ptr, __ATOMIC_ACQUIRE);
}

int pthread_create_soloader(pthread_t *thread, const pthread_attr_t_bionic *attr, void *(*start)(void *), void *param) {
    int ret;

    if (!attr) {
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 512 * 1024);
        ret = pthread_create(thread, &a, start, param);
        pthread_attr_destroy(&a);
    } else{
        _attr_t_static_init((pthread_attr_t_bionic *) attr);
        pthread_attr_setstacksize(attr->real_ptr, 512 * 1024);
        ret = pthread_create(thread, attr->real_ptr, start, param);
    }

    return ret;
}

int pthread_mutexattr_init_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_init(attr);
}

int pthread_mutexattr_settype_soloader(pthread_mutexattr_t *attr, int type)
{
    return pthread_mutexattr_settype(attr, type);
}

int pthread_mutexattr_destroy_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_destroy(attr);
}

int pthread_kill_soloader(pthread_t thread, int sig)
{
    return pthread_kill(thread, sig);
}

int pthread_mutex_init_soloader(pthread_mutex_t_bionic *uid, const pthread_mutexattr_t *attr)
{
    if (!uid) return EINVAL;
    return _mutex_t_init(uid, attr, 1);
}

int pthread_mutex_destroy_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return 0;
    if (_is_mutex_initializer((uintptr_t) mutex->real_ptr)) return 0;
    int ret = pthread_mutex_destroy(mutex->real_ptr);
    free(mutex->real_ptr);
    mutex->real_ptr = 0x0;
    return ret;
}

int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    _mutex_t_init(mutex, NULL, 0);
    return pthread_mutex_lock(_mutex_real(mutex));
}

int pthread_mutex_trylock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    _mutex_t_init(mutex, NULL, 0);
    return pthread_mutex_trylock(_mutex_real(mutex));
}

int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    pthread_mutex_t *real = _mutex_real(mutex);
    if (_is_mutex_initializer((uintptr_t) real)) return EPERM;
    return pthread_mutex_unlock(real);
}

int pthread_join_soloader(pthread_t thread, void **value_ptr)
{
    return pthread_join(thread, value_ptr);
}

int pthread_condattr_init_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_init(attr);
}

int pthread_condattr_destroy_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_destroy(attr);
}

int pthread_cond_init_soloader(pthread_cond_t_bionic *cond,
                               const pthread_condattr_t *attr)
{
    if (!cond) return EINVAL;

    return _cond_t_init(cond, attr, 1);
}

int pthread_cond_destroy_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    if (!cond->real_ptr) return 0;
    int ret = pthread_cond_destroy(cond->real_ptr);
    free(cond->real_ptr);
    cond->real_ptr = 0x0;
    return ret;
}

int pthread_cond_signal_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return EINVAL;

    _cond_t_init(cond, NULL, 0);

    return pthread_cond_signal(_cond_real(cond));
}

int pthread_cond_timedwait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex, struct timespec *abstime)
{
    if (!cond || !mutex) return EINVAL;

    _cond_t_init(cond, NULL, 0);
    _mutex_t_init(mutex, NULL, 0);

    return pthread_cond_timedwait(_cond_real(cond), _mutex_real(mutex), abstime);
}


int pthread_cond_wait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex)
{
    if (!cond || !mutex) return EINVAL;

    _cond_t_init(cond, NULL, 0);
    _mutex_t_init(mutex, NULL, 0);

    return pthread_cond_wait(_cond_real(cond), _mutex_real(mutex));
}

int pthread_cond_broadcast_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return EINVAL;

    _cond_t_init(cond, NULL, 0);

    return pthread_cond_broadcast(_cond_real(cond));
}

int pthread_attr_init_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return EINVAL;

    return _attr_t_static_init(attr);
}

int pthread_attr_destroy_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return 0;
    if (attr->magic != 0x42424242) return 0;

    int ret = pthread_attr_destroy(attr->real_ptr);
    free(attr->real_ptr);
    attr->magic = 0x0;

    return ret;
}

int pthread_attr_setdetachstate_soloader(pthread_attr_t_bionic *attr, int state)
{
    if (!attr) return -1;
    _attr_t_static_init(attr);
    state = !state; // pthread-embedded has JOINABLE/DETACHED swapped compared to BIONIC...
    return pthread_attr_setdetachstate(attr->real_ptr, state);
}

int pthread_attr_setstacksize_soloader(pthread_attr_t_bionic *attr, size_t stacksize) {
    if (!attr) return -1;
    _attr_t_static_init(attr);
    return pthread_attr_setstacksize(attr->real_ptr, stacksize);
}

int pthread_setschedparam_soloader(pthread_t thread, int policy,
                                   const struct sched_param *param)
{
   return pthread_setschedparam(thread, policy, param);
}

int pthread_getschedparam_soloader(pthread_t thread, int *policy,
                                   struct sched_param *param)
{
    return pthread_getschedparam(thread, policy, param);
}

int pthread_detach_soloader(pthread_t thread)
{
    return pthread_detach(thread);
}

int pthread_equal_soloader(const pthread_t t1, const pthread_t t2)
{
    if (t1 == t2)
        return 1;
    if (!t1 || !t2)
        return 0;
    return pthread_equal(t1, t2);
}

pthread_t pthread_self_soloader()
{
    return pthread_self();
}

int pthread_once_soloader(volatile int *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine)
        return -1;
    if (__sync_lock_test_and_set(once_control, 1) == 0)
        (*init_routine)();
    return 0;
}

#ifndef MAX_TASK_COMM_LEN
#define MAX_TASK_COMM_LEN 16
#endif

int pthread_setname_np_soloader(pthread_t thread, const char* thread_name) {
    if (thread == 0 || thread_name == NULL) {
        return EINVAL;
    }
    size_t thread_name_len = strlen(thread_name);
    if (thread_name_len >= MAX_TASK_COMM_LEN) {
        return ERANGE;
    }

    sceClibPrintf("PTHREAD: pthread_setname_np with name %s for thread:0x%x\n", thread_name, pthread_self());

    return 0;
}

int sem_destroy_soloader(int * uid) {
    if (sceKernelDeleteSema(*uid) < 0)
        return -1;
    return 0;
}

int sem_getvalue_soloader (int * uid, int * sval) {
    SceKernelSemaInfo info;
    info.size = sizeof(SceKernelSemaInfo);

    if (sceKernelGetSemaInfo(*uid, &info) < 0) return -1;
    if (!sval) sval = malloc(sizeof(int32_t));
    *sval = info.currentCount;
    return 0;
}

int sem_init_soloader (int * uid, int pshared, unsigned int value) {
    *uid = sceKernelCreateSema("sema", 0, (int) value, 0x7fffffff, NULL);
    if (*uid < 0)
        return -1;
    return 0;
}

int sem_post_soloader (int * uid) {
    if (sceKernelSignalSema(*uid, 1) < 0)
        return -1;
    return 0;
}

int sem_timedwait_soloader (int * uid, const struct timespec * abstime) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) >= 0)
        return 0;
    if (!abstime) return -1;
    long long now = (long long) current_timestamp_ms() * 1000; // us
    long long _timeout = abstime->tv_sec * 1000 * 1000 + abstime->tv_nsec / 1000; // us
    if (_timeout-now >= 0) return -1;
    uint timeout_real = _timeout - now;
    if (sceKernelWaitSema(*uid, 1, &timeout_real) < 0)
        return -1;
    return 0;
}

int sem_trywait_soloader (int * uid) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) < 0)
        return -1;
    return 0;
}

int sem_wait_soloader (int * uid) {
    if (sceKernelWaitSema(*uid, 1, NULL) < 0)
        return -1;
    return 0;
}
