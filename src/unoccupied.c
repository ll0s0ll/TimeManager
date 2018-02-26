/*
 * unoccupied.c
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
 * @file unoccupied.c
 * @brief 空き時間のスケジュールを作成するコマンドに関する実装。
 */

#include "../include/unoccupied.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"

/** 空き時間を検索する範囲の初期値(sec) */
#define DEFAULT_RANGE 3600

/** 空き時間が見つからない場合の戻り値 */
#define EXIT_NOT_FOUND 3

static int verbose = 0;

/**
 * @brief 指定された条件から、空き時間のスケジュールを作成する。
 * @param[in]  shm_name データベース名。
 * @param[in]  begin    開始時刻(time_t)。
 * @param[in]  range    検索範囲(sec)。
 * @param[out] sched    作成したスケジュールが反映される。
 * @return 成功時は0、失敗時には-1、空き時間が見つからない場合は1を返す。
 */
static int generate_unoccupied_sched(const char *shm_name, time_t begin,
				     unsigned int range,
				     struct schedule* sched)
{ 
  // スケジュールデータベースからレコードを読み込む
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    return -1;
  }

  // load_schedules()で不要なスケジュールが削除されるので、
  // ついでにデータベースファイルを更新する。
  if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len) != 0) {
    cleanup_schedules(scheds, scheds_len);
    return -1;
  }

  // 空きスケジュールを取得。
  struct schedule* uo_scheds[MAX_NUM_SCHEDULES];
  size_t uo_len = generate_unoccupied_scheds_from_scheds(scheds,
							 scheds_len,
							 uo_scheds,
							 MAX_NUM_SCHEDULES,
							 begin,
							 range,
							DEFAULT_SCHED_CAPTION);


  if (uo_len == 0) {
    if (verbose) {
      fprintf(stderr, "%s:%d: No unoccupied schedule found.\n", __FILE__,
	      __LINE__);
    }
    cleanup_schedules(scheds, scheds_len);
    return 1;
  }

  // 作成したスケジュールを引数に反映。
  sched->start = uo_scheds[0]->start;
  sched->duration = uo_scheds[0]->duration;
  strcpy(sched->caption, uo_scheds[0]->caption);

  cleanup_schedules(scheds, scheds_len);
  cleanup_schedules(uo_scheds, uo_len);

  return 0;
}


/**
 * @brief stdinの内容をstdoutに受け流す。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int output_input()
{
  while (1) {
    char buf[BUFSIZ];
    size_t num = fread(buf, sizeof(char), BUFSIZ, stdin);
    if (num == 0)
      break;

    size_t num_write = fwrite(buf, sizeof(char), num, stdout);
    fflush(stdout);

    if (num < 512) {
      // エラー
      if (ferror(stdin)) {
	fprintf(stderr, "%s:%d: Error: Reading stdin.\n", __FILE__, __LINE__);
	return -1;
      }

      // 最後まで読み切った。
      if (feof(stdin))
	break;
    }
  }

  return 0;
}


/**
 * @brief inにuoの値を反映したスケジュールをstdoutに出力する。
 * 
 * inのduration値が0以外の場合は、uoのduration値を反映しない。
 *
 * @param[in] in stdinから読み込んだスケジュール。
 * @param[in] uo 反映する空き時間のスケジュール。
 * @return 成功時は0、inのduration値がuoのduration値より大きい場合は1を返す。
 */
static int output_schedule(struct schedule *in, struct schedule *uo)
{
  assert(in != NULL && uo != NULL);

  // 空き時間より継続時間が長い場合はエラー。
  if (in->duration > uo->duration) {
    fprintf(stderr, "%s:%d: Error: Too long duration.\n", __FILE__, __LINE__);
    return 1;
  }

  // 入力されたスケジュールのduration値が0でない場合は、反映させない。
  if (in->duration != 0)
    fprintf(stdout, "%ld:%d:%s\n", uo->start, in->duration, in->caption);
  else
    fprintf(stdout, "%ld:%d:%s\n", uo->start, uo->duration, in->caption);

  fflush(stdout);

  return 0;
}


/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm unoccupied [-b begin] [-d database] [-r range] [-v] "
    "[-h]\n";

  const char *description = "スケジュールが入っていない時間(空き時間)の"
    "スケジュールを作成します。作成したスケジュールは、stdinから読み込んだ"
    "スケジュールに反映し、stdoutに出力します。\n"
    "\n"
    "読み込んだスケジュールの継続時間が、作成した空き時間のスケジュールの"
    "継続時間より大きい場合は、プログラムを終了し、3を返します。\n"
    "\n"
    "読み込んだスケジュールの継続時間が0以外の場合は、作成した空き時間の"
    "スケジュールの継続時間を反映しません。\n"
    "\n"
    "デフォルトの検索開始時刻は、プログラムが実行された時刻です。 また、"
    "デフォルトの検索範囲は3600秒です。\n";
  
  const char *optarg = "OPTIONS\n"
    "\t-b begin    検索開始時刻(time_t形式)\n"
    "\t-d database データベース番号(1-5が使用可能)\n"
    "\t-r range    空き時間を検索する範囲(sec)\n"
    "\t-v          verboseモード\n"
    "\t-h          show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n"
    "\t3 空き時間が見つからない場合\n";

  const char *env = "ENVIRONMENT\n"
    "\tTM_DB_NUM データベース番号(1-5が使用可能)。dオプションが指定された場合は、そちらが優先される。\n";
  
  const char *example = "EXAMPLE\n"
    "\t$ echo \"0:0:caption\" | tm unoccupied\n"
    "\t1517188474:3600:caption\n"
    "\n"
    "\t始めの1行をスケジュールとして読み込み、それ以降はそのまま出力される。\n"
    "\t$ echo -e \"0:0:caption\\nABCDEFG\" | tm unoccupied\n"
    "\t1517188474:3600:caption\n"
    "\tABCDEFG\n";

  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n", usage, description,
	  optarg, exit_status, env, example);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-i'オプション(id値)が反映される。
 * @param[out] opt_d    '-d'オプション(データベース番号)が指定された場合、1が設定される。
 * @param[out] begin    '-b'オプション(検索開始時刻(time_t))の値が反映される。
 * @param[out] range    '-r'オプション(空き時間を検索する範囲(sec))の値が反映される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *opt_d, 
			   time_t* begin, unsigned int *range, int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "unoccupied". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "b:d:hr:v")) != -1) {
    switch (opt) {
    case 'b':
      // 検索開始時刻(time_t)
      *begin = atoi(optarg);
      break;
    case 'd':
      // データベース番号
      if (atoi(optarg) < 1 || atoi(optarg) > MAX_NUM_DB) {
	fprintf(stderr,	"Error: Invalid database number. (Valid 1-%d)\n",
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
    case 'r':
      // 空き時間を検索する範囲(sec)
      *range = atoi(optarg);
      break;
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
 * @brief stdinからスケジュールを読み込む。スケジュールはバリデートされる。
 * @param[out] sched 読み込んだスケジュールが反映される。
 * @return 成功時は0、失敗時には-1、スケジュールが不正な場合は1を返す。
 */
static int read_schedule(struct schedule *sched)
{
  // stdinから1行読み取る。
  char buf[MAX_SCHEDULE_STRING_LEN+1];
  if (fgets(buf, MAX_SCHEDULE_STRING_LEN+1 , stdin) == NULL) {
    if (feof(stdin) == 0) {
      fprintf(stderr, "%s:%d: Error: while reading stdin.\n", __FILE__,
	      __LINE__);
    }
    return -1;
  }

  // 文字列から要素を取得
  char sep0, sep1;
  sscanf(buf, "%ld%c%u%c%[^\n]",
	 &(sched->start), &sep0, &(sched->duration), &sep1, sched->caption);

  // 区切り文字をチェック
  if (sep0 != ':' || sep1 != ':') {
    fprintf(stderr, "%s:%d: Error: Unknown schedule format.\n", __FILE__,
	    __LINE__);
    return 1;
  }

  // 開始時刻がマイナスはあり得ない。
  if (sched->start < 0) {
    fprintf(stderr, "%s:%d: Error: Invalid start value.\n" ,__FILE__,__LINE__);
    return 1;
  }

  if (verbose > 0) {
    fprintf(stderr,
	    "%s:%d: Debug: in start:%ld, dur:%d, caption:%s\n", __FILE__,
	    __LINE__, sched->start, sched->duration, sched->caption);
  }

  return 0;
}


int unoccupied(int argc, char* argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  time_t begin = time(NULL);
  unsigned int range = DEFAULT_RANGE;
  int opt_d = 0;

  // オプションチェック
  switch (parse_arguments(argc, argv, shm_name, &opt_d, &begin, &range,
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

  // stdinからスケジュールを取得する。
  struct schedule sched_in;
  switch (read_schedule(&sched_in)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_MISUSE;
  }

  if (verbose > 0) {
    fprintf(stderr, "%s:%d: Debug: db:%s begin:%ld range:%d\n", __FILE__,
	    __LINE__, shm_name, begin, range);
  }

  // 空き時間のスケジュールを作成。
  struct schedule sched_uo;
  switch (generate_unoccupied_sched(shm_name, begin, range, &sched_uo)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_NOT_FOUND;
  }
  
  // 入力されたスケジュールに、作成したスケジュールを適応して出力する。
  if (output_schedule(&sched_in, &sched_uo) != 0)
      return EXIT_NOT_FOUND;

  // その他のstdinのデータをstdoutに受け流す。
  if (output_input() != 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
