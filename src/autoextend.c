/*
 * autoextend.c
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

#include "../include/autoextend.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/activate.h"
#include "../include/common.h"
#include "../include/lock.h"
#include "../include/unlock.h"

/** 再スケジュールの間隔(sec) */
#define DEFAULT_INTERVAL 1

/** 空き時間を検索する範囲の初期値(sec) */
#define DEFAULT_RANGE 3600

static int verbose = 0;

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm autoextend [-d database] [-i interval] [-r range] "
    "[-v] [-h]\n";
  
  const char *description = "スケジュールの空き状況に応じて、"
    "自動的に現在のスケジュールの継続時間を延長します。\n";

  const char *optarg = "OPTIONS\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-i interval 再スケジュールの間隔(sec)。デフォルトは、1秒。\n"
    "\t-r range    空き時間を検索する範囲(sec)。デフォルトは、3600秒。\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。dオプションが指定された場合は、そちらが優先される。\n";
    
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n", usage, description, optarg,
	  exit_status, env);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-d'オプション(データベース番号)が反映される。
 * @param[out] d_opt    '-d'オプション(データベース番号)が指定された場合、1が設定される。
 * @param[out] interval '-i'オプション(再スケジュールの間隔(sec))の値が反映される。
 * @param[out] range    '-r'オプション(空き時間を検索する範囲(sec))の値が反映される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *d_opt,
			   unsigned int *interval, unsigned int *range,
			   int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "autoextend". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "d:hi:r:v")) != -1) {
    switch (opt) {
    case 'd':
      // データベース番号
      if (atoi(optarg) < 1 || atoi(optarg) > MAX_NUM_DB) {
	fprintf(stderr, "Error: Invalid database number. (Valid 1-%d)\n",
		MAX_NUM_DB);
	return 2;
      }
      strcat(shm_name, optarg);
      *d_opt = 1;
      break;
    case 'i':
      // 再スケジュールする間隔(sec)
      *interval = atoi(optarg);
      break;
    case 'r':
      // 空き時間を検索する範囲(sec)
      *range = atoi(optarg);
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
 * @brief スケジュールの空き状況に応じて、継続時間を延長する。
 * @param[in] sched 延長されるスケジュール
 * @param[in] scheds 空きスケジュール群
 * @param[in] scheds_len 空きスケジュール群の配列数。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int update_schedule(struct schedule *sched, struct schedule* *scheds, int scheds_len)
{
  int i;
  for (i=0; i<scheds_len; i++) {
    if ((sched->start + sched->duration) == scheds[i]->start) {
      sched->duration = (scheds[i]->start + scheds[i]->duration)-sched->start;
    }
  }
  return 0;
}


/**
 * @brief スケジュールの空き状況に応じて、現在のスケジュールの継続時間を、自動的に延長します。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int autoextend(int argc, char* argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  unsigned int interval = DEFAULT_INTERVAL;
  unsigned int range = DEFAULT_RANGE;
  int opt_d = 0;

  // オプションチェック
  switch (parse_arguments(argc, argv, shm_name, &opt_d, &interval, &range,
			  &verbose)) {
  case 1:
    return EXIT_SUCCESS;
  case 2:
    return EXIT_MISUSE;
  }

  // 'd'オプションが指定されていない場合は、環境変数を確認する。
  if (!opt_d) {
    if (get_env(NULL, shm_name) != 0)
      return EXIT_FAILURE;
  }

  if (verbose > 0) {
    fprintf(stderr, "%s:%d: shm_name:%s interval:%d range:%d\n", __FILE__,
	    __LINE__, shm_name, interval, range);
  }

  // execute
  errno = 0;
  pid_t child_pid = fork();
  if (child_pid == -1) {
    //-- Error --//
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return EXIT_FAILURE;
  } else if (child_pid == 0 ) {
    //-- child process --//
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: child pid:%d pgid:%d\n", __FILE__, __LINE__,
	      getpid(), getpgid(0));
    }

    while (1) {

      // semaphore獲得
      if (lock(argc, argv) != 0)
	return -1;

      // スケジュールデータベースからレコードを読み込む
      struct schedule* scheds[MAX_NUM_SCHEDULES];
      size_t scheds_len = 0;
      if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds,
			 MAX_NUM_SCHEDULES, &scheds_len) != 0)
	return -1;

      struct schedule *s = NULL;
      if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) != 0) {
	fprintf(stderr,
		"%s:%d: Error: Could not found schedule for pgid %d.\n",
		__FILE__, __LINE__, getpgid(0));
	cleanup_schedules(scheds, scheds_len);
	return -1;
      }
      if (verbose > 0) {
	fprintf(stderr,
         "%s:%d: org: pgid:%d lock:%d terminator:%d start:%ld dur:%d cap:%s\n",
		__FILE__, __LINE__, s->pgid, s->lock, s->terminator, s->start,
		s->duration, s->caption);
      }

      // 空きスケジュールを取得。
      // 重なりを出すため、検索幅調整。
      time_t start = time(NULL) - interval;
      range = range + interval;
      struct schedule* uo_scheds[MAX_NUM_SCHEDULES];
      size_t uo_scheds_len = generate_unoccupied_scheds_from_scheds(scheds,
								    scheds_len,
								    uo_scheds,
							    MAX_NUM_SCHEDULES,
								    start,
								    range,
								    "");

      // スケジュールを更新
      update_schedule(s, uo_scheds, uo_scheds_len);
      if (verbose > 0) {
	fprintf(stderr,
	"%s:%d: ext: pgid:%d lock:%d terminator:%d start:%ld dur:%d cap:%s\n",
		__FILE__, __LINE__, s->pgid, s->lock, s->terminator, s->start,
		s->duration, s->caption);
      }

      // データベースを更新する。
      if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len)!=0){
	cleanup_schedules(scheds, scheds_len);
	return -1;
      }

      cleanup_schedules(scheds, scheds_len);
      cleanup_schedules(uo_scheds, uo_scheds_len);

      // 適用
      if (activate(argc, argv) != 0) {
	// semaphore解放
	unlock(argc, argv);
	return -1;
      }

      // 再スケジュールまで間隔を開ける。
      sleep(interval);

    } // while

    // ここに到達することはない。
    fprintf(stderr, "%s:%d: Error: Out of loop.\n", __FILE__, __LINE__);
    _exit(127);
      
  } else {
    //-- Parent process --//
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: parent pid:%d pgid:%d\n", __FILE__, __LINE__,
	      getpid(), getpgid(0));
    }
  }

  return EXIT_SUCCESS;
}
