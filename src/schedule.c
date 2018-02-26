/*
 * schedule.c
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
 * @file schedule.c
 * @brief データベースのスケジュールを出力するコマンドに関する実装。
 */

#include "../include/schedule.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"

static int verbose = 0;

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm schedule [-a] [-d database] [-r] [-v] [-h]\n";
  const char *description = "データベースにある有効なスケジュールをstdoutに出"
    "力します。\n";

  const char *optarg = "OPTIONS\n"
    "\t-a          アクティベートされていないスケジュールも出力する。\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-r          データベースの内容をスケジュールフォーマットで出力する。\n"
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
    "\t$ tm schedule\n"
    "\t01/29 10:14-11:14 (1h) caption\n"
    "\n"
    "\t$ tm schedule -r\n"
    "\t1517188474:3600:caption\n";

  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n", usage, description,
	  optarg, exit_status, env, example);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-d'オプション(データベース番号)が反映される。
 * @param[out] opt_a    '-a'オプション(allモード)の値が反映される。
 * @param[out] opt_d    '-d'オプション(データベース番号)が指定された場合、1が設
 * 定される。
 * @param[out] opt_r    '-r'オプション(rawモード)の値が反映される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *opt_a,
			   int *opt_d, int *opt_r, int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "unoccupied". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "ad:rhv")) != -1) {
    switch (opt) {
    case 'a':
      // allモード
      *opt_a = 1;
      break;
    case 'd':
      // データベース番号
      if (atoi(optarg) < 1 || atoi(optarg) > MAX_NUM_DB) {
	fprintf(stderr, "Error: Invalid database number. (Valid 1-%d)\n",
		MAX_NUM_DB);
	return -1;
      }
      strcat(shm_name, optarg);
      *opt_d = 1;
      break;
    case 'h':
      // ヘルプ
      print_usage();
      return 1;
    case 'r':
      // rawモード
      *opt_r = 1;
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
 * @brief データベースにある有効なスケジュールをstdoutに出力します。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int schedule(int argc, char* argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int opt_a = 0, opt_d = 0, opt_r = 0;

  // オプションチェック
  switch (parse_arguments(argc, argv, shm_name, &opt_a, &opt_d, &opt_r,
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
 
  if (verbose > 0)
    fprintf(stderr, "%s:%d: shm_name:%s\n", __FILE__, __LINE__, shm_name);

  // スケジュールデータベースからレコードを読み込む
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return EXIT_FAILURE;
  }

  // スケジュールをstart値で昇順ソート
  sort_schedules(scheds, scheds_len);

  // 書き出し
  int i;
  for (i=0; i<scheds_len; i++) {

    // アクティベートされていないスケジュールは飛ばす。
    if (!opt_a && scheds[i]->terminator == 0) {
      continue;
    }
    
    if (opt_a) {
      struct schedule* s = scheds[i];
      fprintf(stdout, "%d:%d:%d:%ld:%d:%s\n", s->pgid, s->lock, s->terminator,
	      s->start, s->duration, s->caption);
    } else if (opt_r) {
      fprintf(stdout, "%ld:%d:%s\n", scheds[i]->start, scheds[i]->duration,
	      scheds[i]->caption);
    } else {
      // schedule
      struct tm *tm = localtime(&(scheds[i]->start));
      char buf[512];
      if (strftime(buf, sizeof(buf), "%m/%d %H:%M", tm) == 0) {
	fprintf(stderr, "strftime returned 0");
      }
      fprintf(stdout, "%s-", buf);

      time_t end = scheds[i]->start + scheds[i]->duration;
      tm = localtime(&end);
      if (strftime(buf, sizeof(buf), "%H:%M", tm) == 0) {
	fprintf(stderr, "strftime returned 0");
      }
      fprintf(stdout, "%s", buf);

      // duration
      fprintf(stdout, " (");
      div_t d = div(scheds[i]->duration, 3600);
      if (d.quot != 0)
	fprintf(stdout, "%dh", d.quot);
      
      d = div(d.rem, 60);      
      if (d.quot != 0) 
	fprintf(stdout, "%dm", d.quot);

      if (d.rem != 0)
	fprintf(stdout, "%ds", d.rem);

      fprintf(stdout, ")");

      // caption
      fprintf(stdout, " %s\n", scheds[i]->caption);
    }
  }

  fflush(stdout);

  //
  cleanup_schedules(scheds, scheds_len);

  return EXIT_SUCCESS;
}
