/*
 * terminate.c
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
 * @file terminate.c
 * @brief 自プロセスグループを終了させるコマンドに関する実装。
 */

#include "../include/terminate.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
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
  const char *usage = "tm terminate [-d database] [-v] [-h]\n";
  const char *description = "自分が所属するプロセスグループへ"
    "SIGTERMを送信して、プロセスグループに所属するプロセスを終了させます。\n";

  const char *optarg = "OPTIONS\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。"
    "dオプションが指定された場合は、そちらが優先される。\n";

  const char *example = "EXAMPLE\n"
    "\tmyprogramが予定より早く終了した場合、スケジュールを終了させる。\n"
    "\t$ sh -c 'echo \"1517188474:600:cap\" | tm set && myprogram;"
    " tm terminate'\n";
  
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n", usage, description,
	  optarg, exit_status, env, example);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-i'オプション(id値)が反映される。
 * @param[out] opt_d    '-d'オプション(データベース番号)が指定された場合、
 * 1が設定される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *opt_d,
			   int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "terminate". "opt"...}となる。
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
      strcat(shm_name, optarg);
      *opt_d = 1;
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
    default:
      fprintf(stderr, "%s:%d: Error: Unknown option.\n", __FILE__, __LINE__);
      return 2;
    }    
  }

  return 0;
}


/**
 * @brief 自分が所属するプロセスグループへSIGTERMを送信して、
 * プロセスグループに所属するプロセスを終了させる。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int terminate(int argc, char* argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int opt_d = 0;

  // オプション解析
  switch (parse_arguments(argc, argv, shm_name, &opt_d, &verbose)) {
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
    fprintf(stderr, "%s:%d: shm_name:%s\n", __FILE__, __LINE__, shm_name);
  }

  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return EXIT_FAILURE;
  }

  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) != 0) {
    fprintf(stderr, "%s:%d: Error: Could not found schedule for pgid %d.\n",
	    __FILE__, __LINE__, getpgid(0));
    cleanup_schedules(scheds, scheds_len);
    return EXIT_MISUSE;
  }

  pid_t pgid = s->pgid;
  cleanup_schedules(scheds, scheds_len);

  errno = 0;
  if (killpg(pgid, SIGTERM) == -1) {
    fprintf(stderr, "%s:%d: Error: %s. to:%d, sig:%d\n", __FILE__, __LINE__,
	    strerror(errno), pgid, SIGTERM);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
