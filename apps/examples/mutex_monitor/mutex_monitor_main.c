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
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <nuttx/sched.h>
#include <semaphore.h>
#include <sched.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEMO_WORKER_COUNT  5
#define DEMO_ITERATIONS    3
#define DEMO_HOLD_TIME_US  200000
#define DEMO_PAUSE_TIME_US 100000
#define DEMO_MONITOR_INTERVAL_US 50000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct worker_context_s
{
  int id;
  int priority;
  int iterations;
  pthread_t tid;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_demo_mutex = PTHREAD_MUTEX_INITIALIZER;

static sem_t g_low_locked;
static sem_t g_release_low;
static sem_t g_high_attempting;
static sem_t g_demo_ready;
static sem_t g_demo_start;
static sem_t g_demo_done;
static struct worker_context_s g_workers[DEMO_WORKER_COUNT];
static const int g_demo_priorities[DEMO_WORKER_COUNT] =
{
  60, 80, 100, 120, 140
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR void *low_thread(FAR void *arg);
static FAR void *high_thread(FAR void *arg);
static int run_demo(void);
static FAR void *demo_worker(FAR void *arg);

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

  if (argc > 1 && strcmp(argv[1], "demo") == 0)
    {
      return run_demo();
    }

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

/****************************************************************************
 * demo_worker
 ****************************************************************************/

static FAR void *demo_worker(FAR void *arg)
{
  FAR struct worker_context_s *context;
  int iteration = 0;
  int ret;

  context = (FAR struct worker_context_s *)arg;

  ret = sem_post(&g_demo_ready);
  if (ret == -1)
    {
      perror("sem_post g_demo_ready");
      return NULL;
    }

  ret = sem_wait(&g_demo_start);
  if (ret == -1)
    {
      perror("sem_wait g_demo_start");
      goto done;
    }

  for (iteration = 0; iteration < context->iterations; iteration++)
    {
      printf("[worker %d prio %d] Tentativa de aquisição do mutex\n",
            context->id, context->priority);

      ret = pthread_mutex_lock(&g_demo_mutex);
      if (ret != 0)
        {
          printf("pthread_mutex_lock failed: %d\n", ret);
          goto done;
        }

      printf("[worker %d prio %d] Mutex adquirido\n",
            context->id, context->priority);
      usleep(DEMO_HOLD_TIME_US);

      ret = pthread_mutex_unlock(&g_demo_mutex);
      if (ret != 0)
        {
          printf("pthread_mutex_unlock failed: %d\n", ret);
          goto done;
        }

      printf("[worker %d prio %d] Mutex liberado\n",
            context->id, context->priority);
      usleep(DEMO_PAUSE_TIME_US);
    }

done:
  ret = sem_post(&g_demo_done);
  if (ret == -1)
    {
      perror("sem_post g_demo_done");
    }

  return NULL;
}

/****************************************************************************
 * run_demo
 ****************************************************************************/

static int run_demo(void)
{
  pthread_attr_t attr;
  struct sched_param param =
    {
      0
    };

  int created = 0; /* Quantas threads foram criadas com sucesso */
  int index;
  int ret;
  struct mutexinfo_s info;
  int completed = 0;

  if (sem_init(&g_demo_ready, 0, 0) == -1)
    {
      perror("sem_init g_demo_ready");
      return 1;
    }

  if (sem_init(&g_demo_start, 0, 0) == -1)
    {
      perror("sem_init g_demo_start");
      sem_destroy(&g_demo_ready);
      return 1;
    }

  if (sem_init(&g_demo_done, 0, 0) == -1)
    {
      perror("sem_init g_demo_done");
      sem_destroy(&g_demo_start);
      sem_destroy(&g_demo_ready);
      return 1;
    }

  ret = pthread_attr_init(&attr);
  if (ret != 0)
    {
      printf("pthread_attr_init failed: %d\n", ret);
      sem_destroy(&g_demo_ready);
      sem_destroy(&g_demo_start);
      sem_destroy(&g_demo_done);
      return 1;
    }

  for (index = 0; index < DEMO_WORKER_COUNT; index++)
    {
      g_workers[index].id = index;
      g_workers[index].priority = g_demo_priorities[index];
      g_workers[index].iterations = DEMO_ITERATIONS;

      param.sched_priority = g_demo_priorities[index];

      ret = pthread_attr_setschedparam(&attr, &param);
      if (ret != 0)
        {
          printf("pthread_attr_setschedparam failed: %d\n", ret);
          break;
        }

      ret = pthread_create(&g_workers[index].tid, &attr,
                          demo_worker, &g_workers[index]);

      if (ret != 0)
        {
          printf("pthread_create failed: %d\n", ret);
          break;
        }

      created++;
    }

  pthread_attr_destroy(&attr);

  for (index = 0; index < created; index++)
    {
      sem_wait(&g_demo_ready);
    }

  for (index = 0; index < created; index++)
    {
      sem_post(&g_demo_start);
    }

  while (completed < created)
    {
      while (sem_trywait(&g_demo_done) == 0)
        {
          completed++;
        }

      for (index = 0; index < created; index++)
        {
          ret = nxsched_get_mutexinfo(g_workers[index].tid, &info);

          if (ret == OK && info.waiting)
            {
              printf("[monitor] worker=%d tid=%d prio=%d ",
                    g_workers[index].id,
                    (int)g_workers[index].tid,
                    info.waiter_priority);

              printf("espera holder=%d prio=%d risco=%d\n",
                    (int)info.holder_pid,
                    info.holder_priority,
                    info.inversion_risk);
            }
          else if (ret != OK && ret != -ESRCH)
            {
              printf("[monitor] consulta do worker %d falhou: %d\n",
                    g_workers[index].id, ret);
            }
        }

      usleep(DEMO_MONITOR_INTERVAL_US);
    }

  for (index = 0; index < created; index++)
    {
      pthread_join(g_workers[index].tid, NULL);
    }

  sem_destroy(&g_demo_done);
  sem_destroy(&g_demo_start);
  sem_destroy(&g_demo_ready);
  return created == DEMO_WORKER_COUNT ? 0 : 1;
}