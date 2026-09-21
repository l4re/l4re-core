/**
 * \file
 * Priority Inheritance Mutex object.
 */
/*
 * Copyright (C) 2025-2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/types.h>
#include <l4/sys/utcb.h>

/**
 * \defgroup l4_pi_mutex_api Priority Inheritance Mutex C API
 * \{
 * \ingroup  l4_kernel_object_api
 *
 * C interface for controlling priority inheritance mutexes.
 *
 * A priority inheritance mutex (PI mutex) object provides the slow path for a
 * priority-inheritance-aware mutex.
 *
 * User-space implements the fast-path using a compare-and-swap atomic
 * operation (with acquire/release semantics) on the `status` machine-word
 * in kernel-user memory that is associated with the mutex on its creation
 * (see l4_factory_create_pi_mutex()).
 *
 * |  [63/31 .. L4_CAP_SHIFT]  | ... |      0      |
 * |:-------------------------:|:---:|:-----------:|
 * |      thread cap slot      |     | waiter flag |
 *
 * When a thread wants to acquire a PI mutex, it first tries to do so via the
 * fast path: `cas(&status, 0, self_thread_cap)`
 * If the mutex is uncontended the cas succeeds and the thread becomes the owner
 * of mutex. Otherwise, the thread needs to enter the slow path via
 * `l4_pi_mutex_lock()` and the kernel takes over.
 *
 * When a thread wants to release a PI mutex, it first tries to do so via the
 * fast path: `cas(&status, self_thread_cap, 0)`
 * If the mutex is uncontended, i.e. the waiter flag is not set, the cas
 * succeeds. Otherwise, the thread needs to enter the slow path via
 * `l4_pi_mutex_unlock()` and the kernel takes over.
 *
 * \includefile{l4/sys/pi_mutex.h}
 */

/**
 * \ingroup l4_pi_mutex_api
 *
 * Lock contended priority inheritance mutex on slow path.
 *
 * \param mutex      Priority inheritance mutex object.
 * \param timeout    Timeout for blocking the mutex lock operation.
 *                   Note: The receive timeout of this timeout-pair is
 *                   significant for blocking, the send part is usually
 *                   non-blocking.
 *
 * \return Syscall return tag. Use l4_error() to check for errors.
 * \retval 0            Success.
 * \retval -L4_EAGAIN   Lock operation in kernel failed, can happen under
 *                      certain circumstances, try again.
 * \retval -L4_EDEADLK  Deadlock condition was detected or the calling thread
 *                      already owns the mutex.
 * \retval -L4_EINVAL   Invalid parameter or `ku_status` in invalid state.
 *
 * \retval L4_IPC_RETIMEOUT   Timeout expired before mutex could be locked.
 * \retval L4_IPC_RECANCELED  Thread was cancelled while waiting on the mutex.
 */
L4_INLINE l4_msgtag_t
l4_pi_mutex_lock(l4_cap_idx_t mutex, l4_timeout_t timeout) L4_NOTHROW;

/**
 * \internal
 */
L4_INLINE l4_msgtag_t
l4_pi_mutex_lock_u(l4_cap_idx_t mutex, l4_timeout_t timeout,
                   l4_utcb_t *utcb) L4_NOTHROW;

/**
 * \ingroup l4_pi_mutex_api
 *
 * Unlock contended priority inheritance mutex from fast path or slow path.
 *
 * \param mutex      Priority inheritance mutex object.
 *
 * \return Syscall return tag. Use l4_error() to check for errors.
 * \retval 0            Success.
 * \retval -L4_EINVAL   Invalid parameter or `ku_status` in invalid state.
 * \retval -L4_EPERM    Calling thread was not the owner of the mutex.
 */
L4_INLINE l4_msgtag_t
l4_pi_mutex_unlock(l4_cap_idx_t mutex) L4_NOTHROW;

/**
 * \internal
 */
L4_INLINE l4_msgtag_t
l4_pi_mutex_unlock_u(l4_cap_idx_t mutex, l4_utcb_t *utcb) L4_NOTHROW;

/**\} */ /* ends l4_pi_mutex_api group */


/**
 * \ingroup l4_protocol_ops
 *
 * Operations on priority inheritance mutex objects.
 *
 * See #L4_PROTO_PI_MUTEX for the protocol type to use for messages to
 * priority inheritance mutex objects.
 */
enum L4_pi_mutex_ops
{
  L4_PI_MUTEX_LOCK_OP      = 0UL, /**< Lock mutex */
  L4_PI_MUTEX_UNLOCK_OP    = 1UL, /**< Unlock mutex */
};

/* IMPLEMENTATION -----------------------------------------------------------*/

#include <l4/sys/ipc.h>

L4_INLINE l4_msgtag_t
l4_pi_mutex_lock_u(l4_cap_idx_t mutex, l4_timeout_t timeout,
                   l4_utcb_t *utcb) L4_NOTHROW
{
  l4_msg_regs_t *v = l4_utcb_mr_u(utcb);
  v->mr[0] = L4_PI_MUTEX_LOCK_OP;
  return l4_ipc_call(mutex, utcb, l4_msgtag(L4_PROTO_PI_MUTEX, 1, 0, 0),
                     timeout);
}

L4_INLINE l4_msgtag_t
l4_pi_mutex_unlock_u(l4_cap_idx_t mutex, l4_utcb_t *utcb) L4_NOTHROW
{
  l4_msg_regs_t *v = l4_utcb_mr_u(utcb);
  v->mr[0] = L4_PI_MUTEX_UNLOCK_OP;
  return l4_ipc_call(mutex, utcb, l4_msgtag(L4_PROTO_PI_MUTEX, 1, 0, 0),
                     L4_IPC_NEVER);
}


L4_INLINE l4_msgtag_t
l4_pi_mutex_lock(l4_cap_idx_t mutex, l4_timeout_t timeout) L4_NOTHROW
{
  return l4_pi_mutex_lock_u(mutex, timeout, l4_utcb());
}

L4_INLINE l4_msgtag_t
l4_pi_mutex_unlock(l4_cap_idx_t mutex) L4_NOTHROW
{
  return l4_pi_mutex_unlock_u(mutex, l4_utcb());
}
