/*
 * set.c
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
 * @file set.c
 * @brief スケジュールをデータベースに追加、有効化するコマンドに関する実装。
 */

#include "../include/set.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/add.h"
#include "../include/activate.h"
#include "../include/terminate.h"

#include "../include/common.h"

/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm set [-d database] [-s signo] [-v] [-h]\n";

  const char *description = "stdinからスケジュールを読み込み、有効にします。\n"
    "\n"
    "正常に有効化ができると、開始時刻までブロックし、開始時刻とともに残りの"
    "stdinの内容をそのままstdoutに受け流し、終了します。\n"
    "また、終了時刻には、自プロセスグループに指定のシグナルを送信します。"
    "送信されるシグナルのデフォルトはSIGTERMです。\n"
    "\n"
    "スケジュール文字列の書式は start:duration:caption です。"
    "startは、スケジュールの開始時刻(time_t形式)、durationは、継続時間(sec)、"
    "captionは、スケジュールの簡単な説明です。\n";

  const char *optarg = "OPTIONS\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-s signo    終了時刻に送信されるシグナルの番号\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。dオプションが指定された場合は、そちらが優先される。\n";

  const char *example = "EXAMPLE\n"
    "\t2017年8月20日午前7時00分から10分間のスケジュールを作成する。\n"
    "\t$ sh -c 'echo \"1503180600:600:News\" | tm set && myprogram'\n"
    "\n"
    "\t始めの1行をスケジュールとして読み込み、それ以降はそのまま出力される。\n"
    "\t$ sh -c 'echo \"1503180600:600:News\\nABCDEF\" | tm set && myprogram'\n"
    "\tABCDEF\n";

  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n",
	  usage, description, optarg, exit_status, env, example);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "set". "opt"..}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "h")) != -1) {
    switch (opt) {     
    case 'h':
      // ヘルプ
      print_usage();
      return 1;
    case '?':
      //fprintf(stderr, "%s:%d: Error: Unknown option.\n", __FILE__, __LINE__);
      //return -1;
      break;
    default:
      fprintf(stderr, "%s:%d: Error: Unknown option.\n", __FILE__, __LINE__);
      return 2;
    }    
  }
  return 0;
}


int set(int argc, char *argv[])
{
  switch (parse_arguments(argc, argv)) {
  case 1:
    return EXIT_SUCCESS;
  case 2:
    return EXIT_MISUSE;
  }

  if (add(argc, argv) != 0) {
    terminate(argc, argv);
    return EXIT_FAILURE;
  }
  
  if (activate(argc, argv) != 0) {
    terminate(argc, argv);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
