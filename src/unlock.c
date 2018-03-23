/*
 * unlock.c
 * This file is part of TimeManager.
 *
 * Copyright (C) 2018  Shun ITO <shunito.s110@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file unlock.c
 * @brief データベース書き換えアンロックに関する実装。
 */

#include "../include/unlock.h"

#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/common.h"

static int verbose = 0;

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm unlock [-d database] [-v] [-h]\n";

  const char *description = "スケジュールの書き換えロックを解放します。\n";

  const char *optarg = "OPTIONS\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。dオプションが指定された場合は、そちらが優先される。\n";
  
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n",
	  usage, description, optarg, exit_status, env);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] sem_name '-i'オプション(id値)が反映される。
 * @param[out] shm_name '-i'オプション(id値)が反映される。
 * @param[out] d_opt    '-d'オプション(データベース番号)が指定された場合、1が設定される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *sem_name,
			   char *shm_name, int *d_opt, int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "unlock". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "d:hv")) != -1) {
    switch (opt) {
    case 'd':
      // データベース番号
      if (atoi(optarg) < 1 || atoi(optarg) > MAX_NUM_DB) {
	fprintf(stderr, "Error: Invalid database number. (Valid 1-%d)\n",
		MAX_NUM_DB);
	return 2;
      }
      strcat(sem_name, optarg);
      strcat(shm_name, optarg);
      *d_opt = 1;
      break;
    case 'h':
      // ヘルプ
      print_usage();
      return 1;
    case 'v':
      // verboseモード
      *verbose = 1;
      break;
    case '?':
      //fprintf(stderr, "%s:%d: Error: Unknown option.\n", __FILE__, __LINE__);
      //return -1;
      //break;
    default:
      fprintf(stderr, "%s:%d: Error: Unknown option.\n", __FILE__, __LINE__);
      return 2;
    }    
  }

  return 0;
}


/**
 * @brief スケジュールのlock値を戻す。
 * @param[in] shm_name 共有メモリのパス
 * @return 成功時は0、失敗時には-1を返す。
 */
static int undo_lock_value_change(const char *shm_name)
{
  //
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &len) != 0) {
    return -1;
  }

  // ロックしたPGIDからの依頼か確認する。
  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, len, &s) != 0 || s->lock == 1) {
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: pgid:%d has no lock.\n", __FILE__,
	      __LINE__, getpgid(0));
    }
    cleanup_schedules(scheds, len);
    return 0;
  }

  // lock値を変更する。
  s->lock = 1;

  // データベースを更新する。
  if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, len) != 0) {
    cleanup_schedules(scheds, len);
    return -1;
  }

  cleanup_schedules(scheds, len);

  return 0;
}


int unlock(int argc, char* argv[])
{
  char sem_name[NAME_MAX-4] = DEFAULT_SEMAPHORE_NAME;
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int opt_d = 0;

  // オプションチェック
  switch (parse_arguments(argc, argv, sem_name, shm_name, &opt_d, &verbose)) {
  case 1:
    return EXIT_SUCCESS;
  case 2:
    return EXIT_MISUSE;
  }

  // 'd'オプションが指定されていない場合は、環境変数を確認する。
  if (!opt_d) {
    if (get_env(sem_name, shm_name) != 0)
      return EXIT_FAILURE;
  }

  if (verbose > 0) {
    fprintf(stderr, "%s:%d: DEBUG: sem_name:%s shm_name:%s\n", __FILE__,
	    __LINE__, sem_name, shm_name);
  }

  //
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return EXIT_FAILURE;
  }

  // ロックしたPGIDからの依頼か確認する。
  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) != 0 ||
      s->lock == 0) {
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: pgid:%d has no lock.\n", __FILE__,
	      __LINE__, getpgid(0));
    }
    cleanup_schedules(scheds, scheds_len);
    return EXIT_SUCCESS;
  }

  // lock値を変更する。
  s->lock = 0;

  // データベースを更新する。
  if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len) != 0) {
    cleanup_schedules(scheds, scheds_len);
    return EXIT_FAILURE;
  }

  cleanup_schedules(scheds, scheds_len);

  // 
  errno = 0;
  sem_t *sem = sem_open(sem_name, 0);
  if (sem == SEM_FAILED) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    undo_lock_value_change(shm_name);
    return EXIT_FAILURE;
  }
  
  errno = 0;
  if (sem_post(sem) == -1) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    undo_lock_value_change(shm_name);
    return EXIT_FAILURE;
  }

  errno = 0;
  if (sem_close(sem) == -1) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return EXIT_FAILURE;
  }

  if (verbose > 0)
    fprintf(stderr, "%s:%d: DEBUG: Release semaphore.\n", __FILE__, __LINE__);

  return EXIT_SUCCESS;
}
