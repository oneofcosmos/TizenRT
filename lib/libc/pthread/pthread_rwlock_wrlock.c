/****************************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * lib/libc/pthread/pthread_rwlock_wrlock.c
 *
 *   Copyright (C) 2017 Mark Schulte. All rights reserved.
 *   Author: Mark Schulte <mark@mjs.pw>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/semaphore.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_PTHREAD_CLEANUP
static void wrlock_cleanup(FAR void *arg)
{
	FAR pthread_rwlock_t *rw_lock = (FAR pthread_rwlock_t *) arg;

	rw_lock->num_writers--;
	(void)pthread_mutex_unlock(&rw_lock->lock);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pthread_rwlock_wrlock
 *
 * Description:
 *   Locks a read/write lock for writing
 *
 * Parameters:
 *   None
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

int pthread_rwlock_trywrlock(FAR pthread_rwlock_t *rw_lock)
{
	int err = pthread_mutex_trylock(&rw_lock->lock);

	if (err != 0) {
		return err;
	}

	if (rw_lock->num_readers > 0 || rw_lock->write_in_progress) {
		err = EBUSY;
	} else {
		rw_lock->write_in_progress = true;
	}

	pthread_mutex_unlock(&rw_lock->lock);
	return err;
}

int pthread_rwlock_timedwrlock(FAR pthread_rwlock_t *rw_lock, FAR const struct timespec *ts)
{
	int err = pthread_mutex_lock(&rw_lock->lock);

	if (err != 0) {
		return err;
	}

	if (rw_lock->num_writers == UINT_MAX) {
		err = EAGAIN;
		goto exit_with_mutex;
	}

	rw_lock->num_writers++;

#ifdef CONFIG_PTHREAD_CLEANUP
	pthread_cleanup_push(&wrlock_cleanup, rw_lock);
#endif
	while (rw_lock->write_in_progress || rw_lock->num_readers > 0) {
		if (ts != NULL) {
			err = pthread_cond_timedwait(&rw_lock->cv, &rw_lock->lock, ts);
		} else {
			err = pthread_cond_wait(&rw_lock->cv, &rw_lock->lock);
		}

		if (err != 0) {
			break;
		}
	}
#ifdef CONFIG_PTHREAD_CLEANUP
	pthread_cleanup_pop(0);
#endif

	if (err == 0) {
		rw_lock->write_in_progress = true;
	} else {
		/* In case of error, notify any blocked readers. */

		(void)pthread_cond_broadcast(&rw_lock->cv);
	}

	rw_lock->num_writers--;

exit_with_mutex:
	pthread_mutex_unlock(&rw_lock->lock);
	return err;
}

int pthread_rwlock_wrlock(FAR pthread_rwlock_t *rw_lock)
{
	return pthread_rwlock_timedwrlock(rw_lock, NULL);
}
