/****************************************************************************
 * sched/sched/sched_get_mutexinfo.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>

#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/sched.h>

#include "sched/sched.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxsched_get_mutexinfo
 *
 * Description:
 *   Report whether a thread is waiting for a mutex, the mutex holder and
 *   their current priorities, and whether the priorities indicate a
 *   possible priority inversion.
 *
 * Input Parameters:
 *   pid       - Identifies the thread to query. Zero identifies the
 *               calling thread.
 *   info      - User-provided location to return the mutex wait information.
 *
 * Returned Value:
 *   Zero (OK) if successful.  Otherwise, a negated errno value is returned.
 *   A valid thread that is not waiting for a mutex also returns OK when:
 *   waiting = false
 *   holder_pid = INVALID_PROCESS_ID
 *   holder_priority = -1
 *   inversion_risk = false
 *
 *     -EINVAL  The info argument is NULL.
 *     -ESRCH   The pid does not identify an active thread.
 *
 ****************************************************************************/

int nxsched_get_mutexinfo(pid_t pid, FAR struct mutexinfo_s *info)
{
  FAR struct tcb_s *tcb;   /* TCB of queried task */
  FAR struct tcb_s *htcb;  /* TCB of mutex holder */
  int ret = OK;
  irqstate_t flags;
  FAR sem_t *sem;

  if (info != NULL)
    {
      flags = enter_critical_section();

      /* Pid of 0 means that we are querying ourself */

      if (pid == 0)
        {
          /* We can always query ourself */

          tcb = this_task();
        }
      else
        {
          /* Get the task to be queried */

          tcb = nxsched_get_tcb(pid);
        }

      if (tcb == NULL)
        {
          ret = -ESRCH;
        }
      else
        {
          info->holder_pid = INVALID_PROCESS_ID;
          info->waiter_priority = (int)tcb->sched_priority;
          info->holder_priority = -1;
          info->waiting = false;
          info->inversion_risk = false;

          if (tcb->task_state == TSTATE_WAIT_SEM && tcb->waitobj != NULL)
            {
              sem = (FAR sem_t *)tcb->waitobj;

              if ((sem->flags & SEM_TYPE_MUTEX) != 0)
                {
                  info->waiting = true;
                  info->holder_pid = nxmutex_get_holder((FAR mutex_t *)sem);
                }
            }

          if (info->holder_pid >= 0)
            {
              htcb = nxsched_get_tcb(info->holder_pid);

              if (htcb != NULL)
                {
                  info->holder_priority = (int)htcb->sched_priority;
                  info->inversion_risk = info->waiter_priority >
                                        info->holder_priority;
                }
            }
        }

      leave_critical_section(flags);
    }
  else
    {
      ret = -EINVAL;
    }

  return ret;
}
