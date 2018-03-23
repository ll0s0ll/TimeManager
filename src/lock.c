/*
 * lock.c
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
 * @file lock.c
 * @brief データベース書き換えロックに関する実装。
 */

#include "../include/lock.h"

#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // for O_CREAT,S_IRUSR,S_IWUSR
#include <unistd.h>

#include "../include/common.h"

/** セマフォ取得待ちのタイムアウトのデフォルト値。(sec)*/
#define DEFAULT_TIMEOUT 5

/** タイムアウトした場合の終了ステータス */
#define EXIT_TIMEDOUT 3

static int verbose = 0;

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm lock [-d database] [-t timeout] [-v] [-h]\n";

  const char *description = "スケジュールの書き換えをロックします。\n"
    "\n"
    "すでに他のプロセスによってロックされている場合は、"
    "ロックが解除されるまで待ちます。"
    "指定時間以内にロックが解除されない場合は、タイムアウトします。"
    "タイムアウトのデフォルト値は5秒です。\n";

  const char *optarg = "OPTIONS\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-t timeout  ロック取得待ちのタイムアウト時間(sec)。\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n"
    "\t3 タイムアウトした場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。dオプションが指定された場合は、そちらが優先される。\n";
  
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n", usage, description, optarg,
	  exit_status, env);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] sem_name '-d'オプション(データベース番号)が反映される。
 * @param[out] shm_name '-d'オプション(データベース番号)が反映される。
 * @param[out] d_opt    '-d'オプション(データベース番号)が指定された場合、1が設定される。
 * @param[out] timeout '-t'オプション(タイムアウトまでの時間)の値が反映される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *sem_name,
			   char *shm_name, int *d_opt, unsigned int *timeout,
			   int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "lock". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "d:ht:v")) != -1) {
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
    case 't':
      // セマフォ取得のタイムアウトするまでの時間(sec)
      *timeout = atoi(optarg);
      break;
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
 * @brief すでにロックを取得していないか確認する。
 * @param[in] pgid 確認するpgid。
 * @param[in] shm_name 共有メモリのオブジェクト名。
 * @return 取得していない時は0、すでに取得している時は1、失敗時には-1を返す。
 */
static int check_repetition_locking(pid_t pgid, const char *shm_name)
{
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return -1;
  }

  struct schedule *s = NULL;
  if (find_sched_by_pgid(pgid, scheds, scheds_len, &s) == 0 && s->lock == 1) {
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: pgid:%d already has lock.\n", __FILE__,
	      __LINE__, getpgid(0));
    }
    cleanup_schedules(scheds, scheds_len);
    return 1;
  }

  cleanup_schedules(scheds, scheds_len);

  return 0;
}

/**
 * @brief シグナルハンドラ。特に何もしない。
 */
static void sigalrm_handler(int sig, siginfo_t *siginfo, void *ucontext)
{
  // Do nothing.
  ;
  //fprintf(stderr, "%s:%d: DEBUG: sigalrm_handler signal:%d\n",
  //  __FILE__, __LINE__, getpid(), siginfo->si_signo);
}


/**
 * @brief SIGALRMのシグナルハンドラを変更する。
 * @param[out] sa_org 変更前のsigaction値。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int setup_sigalrm_handler(struct sigaction *sa_org)
{
  struct sigaction sa;
  sa.sa_sigaction = sigalrm_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGALRM, &sa, sa_org) != 0) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return -1;
  }  
  return 0;
}


/**
 * @brief SIGALRMのシグナルハンドラを戻す。
 * @param[in] sa_org 設定するsigaction値。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int restore_sigalrm_handler(struct sigaction *sa_org)
{
  if (sigaction(SIGALRM, sa_org, NULL) != 0) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return -1;
  }  
  return 0;
}


int lock(int argc, char* argv[])
{
  char sem_name[NAME_MAX-4] = DEFAULT_SEMAPHORE_NAME;
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int opt_d = 0;
  unsigned int timeout = DEFAULT_TIMEOUT;

  // オプションチェック
  switch (parse_arguments(argc, argv, sem_name, shm_name, &opt_d, &timeout,
			  &verbose)) {
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
    fprintf(stderr, "%s:%d: DEBUG: sem_name:%s shm_name:%s timeout:%d\n",
	    __FILE__, __LINE__, sem_name, shm_name, timeout);
  }

  // 同じPGIDから重複して依頼があった場合は、何もしない。
  switch (check_repetition_locking(getpgid(0), shm_name)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_SUCCESS;
  }

  //--- セマフォ関連 ---//  

  errno = 0;
  sem_t *sem = sem_open(sem_name, O_CREAT, S_IRUSR|S_IWUSR, 1);
  if (sem == SEM_FAILED) {
    fprintf(stderr, "%s:%d: Error: sem_open() %s.\n", __FILE__, __LINE__,
	    strerror(errno));
    return EXIT_FAILURE;
  }

  // タイムアウト時間を通知するシグナルを設定。
  struct sigaction sa_org;
  setup_sigalrm_handler(&sa_org);
  alarm(timeout);

  errno = 0;
  if (sem_wait(sem) == -1) {
    if (errno == EINTR) {
      fprintf(stderr, "%s:%d: Error: Timed out. %s.\n", __FILE__, __LINE__,
	      strerror(errno));
      return EXIT_TIMEDOUT;
    }
    fprintf(stderr, "%s:%d: Error: sem_wait() %s.\n", __FILE__, __LINE__,
	    strerror(errno));
    return EXIT_FAILURE;
  }

  alarm(0);
  restore_sigalrm_handler(&sa_org);

  errno = 0;
  if (sem_close(sem) == -1) {
    fprintf(stderr, "%s:%d: Error: sem_close() %s.\n", __FILE__, __LINE__,
	    strerror(errno));
    return EXIT_FAILURE;
  }

  //--- スケジュール関連 ---//

  // スケジュールデータベースを読み出す。
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return EXIT_FAILURE;
  }

  // データベースに自プロセスグループのスケジュールがあるか確認。
  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) == 0) {
    // スケジュールあり。lock値を変更。
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: Found record. Update lock value.\n",
	      __FILE__, __LINE__);
    }

    s->lock = 1;

  } else {
    // スケジュールなし。新規作成。
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: Not found record. Create new record.\n",
	      __FILE__, __LINE__);
    }
    
    if (scheds_len == MAX_NUM_SCHEDULES) {
      fprintf(stderr, "%s:%d: Error: Too many schedules.\n",__FILE__,__LINE__);
      cleanup_schedules(scheds, scheds_len);
      return EXIT_FAILURE;
    }

    struct schedule* s;
    if (create_schedule(getpgid(0), 1, 0, 0, 0, DEFAULT_SCHED_CAPTION, &s)!=0){
      cleanup_schedules(scheds, scheds_len);
      return EXIT_FAILURE;
    }

    if (verbose > 0) {
      fprintf(stderr,
      "%s:%d: DEBUG: new record: pgid:%d lock:%d start:%ld dur:%d cap:%s\n",
	      __FILE__, __LINE__, s->pgid, s->lock, s->start, s->duration,
	      s->caption);
    }
    
    //
    scheds[scheds_len] = s;
    scheds_len++;
  }

  // データベースファイルを更新する。
  if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len) != 0) {
    cleanup_schedules(scheds, scheds_len);
    return EXIT_FAILURE;
  }

  cleanup_schedules(scheds, scheds_len);

  return EXIT_SUCCESS;
}
