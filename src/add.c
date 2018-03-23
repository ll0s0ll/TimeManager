/*
 * add.c
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
 * @file add.c
 * @brief データベースにスケジュールを追加するコマンドに関する実装。
 */

#include "../include/add.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"
#include "../include/lock.h"
#include "../include/unlock.h"

static int verbose = 0;

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm add [-d database] [-v] [-h]\n";

  const char *description = "stdinからスケジュール文字列を読み取り、スケジュ"
    "ールデータベースへ追加します。\n"
    "\n"
    "スケジュール文字列の書式は start:duration:caption です。"
    "startは、スケジュールの開始時刻(time_t形式)、durationは、継続時間(sec)、"
    "captionは、スケジュールの簡単な説明です。\n"
    "\n"
    "すでに自プロセスグループのスケジュールが存在する場合は、上書きします。\n";

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

  const char *example = "EXAMPLE\n"
    "\t$ sh -c 'echo \"1503180600:600:今朝のニュース\" | tm add && tm activate && myprogram; tm terminate;'\n";
  
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n",
	  usage, description, optarg, exit_status, env, example);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-d'オプション(データベース番号)が反映される。
 * @param[out] d_opt    '-d'オプション(データベース番号)が指定された場合、1が設
 * 定される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *d_opt,
			   int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "add". "opt"...}となる。
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
	return -1;
      }
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
 * @brief stdinからスケジュールを読み込む。
 * 
 * stdinから始めの1行をスケジュールとして読み込む。
 * 読み込んだスケジュールの終了時刻が、現在時刻よりも過去の場合は1を返す。
 *
 * @param[out] sched 読み込んだスケジュールが反映される。
 * @return 成功時は0、失敗時には-1、不正なスケジュールの場合は1を返す。
 */
static int read_schedule(struct schedule* *sched)
{
  // stdinから1行読み取る。
  char buf[MAX_SCHEDULE_STRING_LEN+1];
  if (fgets(buf, MAX_SCHEDULE_STRING_LEN+1 , stdin) == NULL) {   
    if (feof(stdin) == 0) {
      fprintf(stderr, "%s:%d: Error: while reading stdin.\n", __FILE__,
	      __LINE__);
      return -1;
    }
  }

  char str[MAX_SCHEDULE_STRING_LEN];
  sprintf(str, "%d:%d:%d:%s", getpgid(0), 0, 0, buf);

  if (string_to_schedule(str, sched) != 0) {
    return 1;
  }
  
  time_t current = time(NULL);
  if (((*sched)->start + (*sched)->duration) < current) {
    fprintf(stderr, "%s:%d: Error: past schedule. current:%ld, new_end:%ld\n",
	    __FILE__, __LINE__, current, ((*sched)->start+(*sched)->duration));
    free((*sched));
    return 1;
  }
  
  if (verbose > 0) {
    fprintf(stderr,
	    "%s:%d: debug: in start:%ld, duration:%d, caption:%s\n", __FILE__,
	    __LINE__, (*sched)->start, (*sched)->duration, (*sched)->caption);
  }
  
  return 0;
}


int add(int argc, char *argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int d_opt = 0;

  // オプション解析
  switch (parse_arguments(argc, argv, shm_name, &d_opt, &verbose)) {
  case 1:
    return EXIT_SUCCESS;
  case 2:
    return EXIT_MISUSE;
  }

  // 'd'オプションが指定されていない場合は、環境変数を確認する。
  if (!d_opt) {
    if (get_env(NULL, shm_name) != 0)
      return EXIT_FAILURE;
  }
 
  if (verbose > 0)
    fprintf(stderr, "%s:%d: shm_name:%s\n",__FILE__, __LINE__, shm_name);

  // stdinからスケジュールを読み込む。
  struct schedule* new;
  switch (read_schedule(&new)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_MISUSE;
  }

  // データベースをロックする。
  if (lock(argc, argv) != 0)
    return EXIT_FAILURE;

  // 既存のスケジュール取得
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    free(new);
    unlock(argc, argv);
    return EXIT_FAILURE;
  }

  // 重複チェック
  if (check_sched_conflict(new, scheds, scheds_len) != 0) {
    fprintf(stderr, "%s:%d: Error: Double booking.\n", __FILE__, __LINE__);
    cleanup_schedules(scheds, scheds_len);
    free(new);
    unlock(argc, argv);
    return EXIT_FAILURE;
  }
  
  // すでにスケジュールがあるか。
  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) == 0) {
    // スケジュールあり。上書き。
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: Find record. update lock value.\n", __FILE__,
	      __LINE__);
    }

    s->start = new->start;
    s->duration = new->duration;
    strcpy(s->caption, new->caption);

    free(new);

  } else {
    // スケジュールなし。追加。
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: Not found record. Create new record.\n",
	      __FILE__, __LINE__);
    }

    scheds[scheds_len] = new;
    scheds_len++;
  }

  // データベースファイルを更新する。
  if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len) != 0) {
    cleanup_schedules(scheds, scheds_len);
    unlock(argc, argv);
    return EXIT_FAILURE;
  }

  cleanup_schedules(scheds, scheds_len);

  // データベースをアンロックする。
  if (unlock(argc, argv) != 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
