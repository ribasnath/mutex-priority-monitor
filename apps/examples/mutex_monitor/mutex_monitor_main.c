/****************************************************************************
 * apps/examples/mutex_monitor/mutex_monitor_main.c
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
#include <stdio.h>
#include <pthread.h>
#include <nuttx/sched.h>
#include <semaphore.h>
#include <sched.h>
#include <errno.h>

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static sem_t g_low_locked;
static sem_t g_release_low;
static sem_t g_high_attempting;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *low_thread(FAR void *arg);
static FAR void *high_thread(FAR void *arg);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * mutex_monitor_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;
  pthread_t low_tid;
  pthread_t high_tid;
  pthread_attr_t attr;
  struct sched_param param =
    {
      0
    };

  struct mutexinfo_s info;

  /* Inicia os semáforos */

  if (sem_init(&g_low_locked, 0, 0) == -1)
    {
      perror("sem_init g_low_locked");
      return 1;
    }

  if (sem_init(&g_release_low, 0, 0) == -1)
    {
      perror("sem_init g_release_low");
      sem_destroy(&g_low_locked);
      return 1;
    }

  if (sem_init(&g_high_attempting, 0, 0) == -1)
    {
      perror("sem_init g_high_attempting");
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  /* Inicia pthread low */

  ret = pthread_attr_init(&attr);
  if (ret != 0)
    {
      printf("pthread_attr_init failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  param.sched_priority = 80;

  ret = pthread_attr_setschedparam(&attr, &param);
  if (ret != 0)
    {
      printf("pthread_attr_setschedparam failed: %d\n", ret);
      pthread_attr_destroy(&attr);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  ret = pthread_create(&low_tid, &attr, low_thread, NULL);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      printf("pthread_create failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  ret = sem_wait(&g_low_locked);
  if (ret == -1)
    {
      perror("sem_wait g_low_locked");
      return 1;
    }

  printf("[main] low confirmou a aquisição\n");

  /* Inicia pthread high */

  ret = pthread_attr_init(&attr);
  if (ret != 0)
    {
      printf("pthread_attr_init failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  param.sched_priority = 150;

  ret = pthread_attr_setschedparam(&attr, &param);
  if (ret != 0)
    {
      printf("pthread_attr_setschedparam failed: %d\n", ret);
      pthread_attr_destroy(&attr);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  ret = pthread_create(&high_tid, &attr, high_thread, NULL);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      printf("pthread_create failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  ret = sem_wait(&g_high_attempting);
  if (ret == -1)
    {
      perror("sem_wait g_high_attempting");
      return 1;
    }

  printf("[main] high iniciou a tentativa\n");

  /* Chama nova syscall */

  ret = nxsched_get_mutexinfo(high_tid, &info);
  if (ret < 0)
    {
      printf("nxsched_get_mutexinfo failed: %d\n", ret);
    }
  else
    {
      printf("TID consultado=%d ", (int)high_tid);
      printf("Waiting?=%d ", info.waiting);
      printf("Prioridade do waiter=%d\n", info.waiter_priority);
      printf("PID do holder=%d ", info.holder_pid);
      printf("Prioridade do holder=%d ", info.holder_priority);
      printf("Risco de inversão?=%d\n", info.inversion_risk);
    }

  ret = nxsched_get_mutexinfo((pid_t)high_tid, NULL);
  if (ret == -EINVAL)
    {
      printf("[TEST] NULL info: PASS\n");
    }
  else
    {
      printf("[TEST] NULL info: FAIL, ret=%d\n", ret);
    }

  ret = nxsched_get_mutexinfo(INVALID_PROCESS_ID, &info);
  if (ret == -ESRCH)
    {
      printf("[TEST] invalid PID: PASS\n");
    }
  else
    {
      printf("[TEST] invalid PID: FAIL, ret=%d\n", ret);
    }

  ret = sem_post(&g_release_low);
  if (ret == -1)
    {
      perror("sem_post g_release_low");
      return 1;
    }

  ret = nxsched_get_mutexinfo(0, &info);
  if (ret == OK && !info.waiting &&
      info.holder_pid == INVALID_PROCESS_ID &&
      info.holder_priority == -1 && !info.inversion_risk)
    {
      printf("[TEST] running task: PASS, priority=%d\n",
            info.waiter_priority);
    }
  else
    {
      printf("[TEST] running task: FAIL, ret=%d\n", ret);
    }

  ret = pthread_join(high_tid, NULL);
  if (ret != 0)
    {
      printf("pthread_join failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  ret = pthread_join(low_tid, NULL);
  if (ret != 0)
    {
      printf("pthread_join failed: %d\n", ret);
      sem_destroy(&g_high_attempting);
      sem_destroy(&g_release_low);
      sem_destroy(&g_low_locked);
      return 1;
    }

  sem_destroy(&g_high_attempting);
  sem_destroy(&g_release_low);
  sem_destroy(&g_low_locked);
  return 0;
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * low_thread
 ****************************************************************************/

static FAR void *low_thread(FAR void *arg)
{
  int ret;

  (void)arg;

  ret = pthread_mutex_lock(&g_mutex);
  if (ret != 0)
    {
      printf("pthread_mutex_lock failed: %d\n", ret);
      return NULL;
    }

  printf("[low] mutex adquirido\n");

  ret = sem_post(&g_low_locked);
  if (ret == -1)
    {
      perror("sem_post g_low_locked");
      pthread_mutex_unlock(&g_mutex);
      return NULL;
    }

  ret = sem_wait(&g_release_low);
  if (ret == -1)
    {
      perror("sem_wait g_release_low");
      pthread_mutex_unlock(&g_mutex);
      return NULL;
    }

  ret = pthread_mutex_unlock(&g_mutex);
  if (ret != 0)
    {
      printf("pthread_mutex_unlock failed: %d\n", ret);
      return NULL;
    }

  printf("[low] mutex liberado\n");
  return NULL;
}

/****************************************************************************
 * high_thread
 ****************************************************************************/

static FAR void *high_thread(FAR void *arg)
{
  int ret;

  (void)arg;

  ret = sem_post(&g_high_attempting);
  if (ret == -1)
    {
      perror("sem_post g_high_attempting");
      return NULL;
    }

  printf("[high] tentando adquirir mutex\n");

  ret = pthread_mutex_lock(&g_mutex);
  if (ret != 0)
    {
      printf("pthread_mutex_lock failed: %d\n", ret);
      return NULL;
    }

  printf("[high] mutex adquirido\n");

  ret = pthread_mutex_unlock(&g_mutex);
  if (ret != 0)
    {
      printf("pthread_mutex_unlock failed: %d\n", ret);
      return NULL;
    }

  printf("[high] mutex liberado\n");
  return NULL;
}
