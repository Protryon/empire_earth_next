// SPDX-License-Identifier: MIT-Modern-Variant
//
// The C11 mutexes dmusic uses (Thread.h declares the c11threads set), over POSIX threads. Written for this vendored
// copy (empire_earth_next): the upstream Thread.Posix.c was not vendored, and Linux's glibc supplied the functions
// from its own C11 threads, which macOS does not have. Only the mutex functions are defined: dmusic calls no other
// (mtx_init/destroy/lock/unlock). Compiles to nothing on Windows (Thread.Win32.c).
#ifndef _WIN32

#include "Thread.h"

int mtx_init(mtx_t* mtx, int type)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if (type & mtx_recursive)
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	int res = pthread_mutex_init(mtx, &attr);
	pthread_mutexattr_destroy(&attr);
	return res == 0 ? thrd_success : thrd_error;
}

void mtx_destroy(mtx_t* mtx)
{
	pthread_mutex_destroy(mtx);
}

int mtx_lock(mtx_t* mtx)
{
	return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int mtx_unlock(mtx_t* mtx)
{
	return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

#endif
