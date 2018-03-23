/*
 * crontab.c
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
 * @file crontab.c
 * @brief crontab形式で指定した開始時刻を取得するコマンドに関する実装。
 */

#include "../include/crontab.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"

#define MAIN_PROGRAM // For cron.h
#include "../include/crontab_cron.h"

/** 空き時間が見つからない場合の戻り値 */
#define EXIT_NOT_FOUND 3

static int verbose = 0;

static void print_usage();

/**
 * @brief entry構造体を解析して、直近の時刻を取得する。
 * @param[out] result 取得した時刻が反映される。
 * @param[in]  e      解析するentry構造体へのポインタ。
 * @param[in]  start  検索を開始する時刻(time_t)
 * @param[in]  range  検索する範囲(sec)
 * @return 成功時は0、失敗時には-1を返す。
 */
static int attack(time_t *result,  struct _entry *e, time_t start,
		  unsigned int range)
{
  assert(e != NULL);
  
  // 1分ごとにこつこつ調べる。
  time_t head;
  for (head=start; head<=(start+range); head+=60) {

    // tm構造体に変換して、要素ごとに取り出しやすくする。
    // 秒単位は0に丸める。
    struct tm *c = localtime(&head);
    c->tm_sec = 0;
    //fprintf(stderr, "current:%s", asctime(c));
    
    // month
    if (!bit_test(e->month, c->tm_mon)) { 
      //fprintf(stderr, "month\n");
      continue;
    }
    
    // DOM and DOW //tm_mdayが(1-31)のため-1する。
    if (!( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
	   ? (bit_test(e->dow, c->tm_wday) && bit_test(e->dom, c->tm_mday-1))
	   : (bit_test(e->dow, c->tm_wday) || bit_test(e->dom, c->tm_mday-1))
	   )) {
      //fprintf(stderr, "DOM\n");
      continue;
    }
    
    // hour
    if (!bit_test(e->hour, c->tm_hour)) {
      //fprintf(stderr, "HOUR:%d\n", hour);
      continue;
    }
    
    // minute
    if (!bit_test(e->minute, c->tm_min)) {
      //fprintf(stderr, "MINUTE:%d\n", minute);
      continue;
    }
 
    *result = mktime(c);
    return 0;
  }
  
  return -1;
}


/**
 * @brief stdinの内容をstdoutに受け流す。
 * @param[in] in stdinから読み込んだスケジュール。
 * @param[in] uo 反映する空き時間のスケジュール。
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
 * @brief inにstartを反映したスケジュールをstdoutに出力する。
 * @param[in] in    stdinから読み込んだスケジュール。
 * @param[in] start 反映する開始時刻。
 */
static void output_schedule(struct schedule *in, time_t start)
{
  assert(in != NULL);
  
  fprintf(stdout, "%ld:%d:%s\n", start, in->duration, in->caption);
  fflush(stdout);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc           argc値
 * @param[in]  argv           argv値
 * @param[out] arg            位置引数の値が反映される。
 * @param[out] range_backward '-r'オプションの値が反映される。
 * @param[out] range_forward  '-R'オプションの値が反映される。
 * @param[out] verbose        '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char **arg,
			   unsigned int *range_backward,
			   unsigned int *range_forward, int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "crontab". "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  //optind = 1;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "hR:r:v")) != -1) {
    switch (opt) {
    case 'r':
      // 実行時刻を基準とした時刻を検索する過去の範囲(sec)
      *range_backward = atoi(optarg);
      break;
    case 'R':
      // 実行時刻を基準とした時刻を検索する未来の範囲(sec)
      *range_forward = atoi(optarg);
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

  // 引数が足りない。
  if (optind == argc) {
    print_usage();
    fprintf(stderr, "Error: Missing positional argument.\n");
    return 2;
  }

  // 位置引数の値を保存。
  *arg = argv[optind];

  // さらに引数が指定されても無視する。

  return 0;
}


/**
 * @brief ファイルに書き込まれたcrontabフォーマットの文字列を解析してentry構造体を作成する。
 *
 * 作成された構造体は、不要になったときメモリの解放をする必要がある。
 *
 * @param[in] fp 解析するcrontabフォーマットの文字列が書き込まれたファイルへのポインタ。
 * @return 成功時にはentry構造体のポインタを、失敗時にはNULLを返す。
 */
static entry* parse_string(FILE *fp)
{
  entry *e = (entry *) calloc(sizeof(entry), sizeof(char));
  if (e == NULL) {
    fprintf(stderr, "%s:%d: Error: out of memory.\n", __FILE__, __LINE__);
    return NULL;
  }

  int ch = get_char(fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: nothing to read.\n", __FILE__, __LINE__);
    goto eof;
  }

  // minute
  ch = get_list(e->minute, FIRST_MINUTE, LAST_MINUTE, PPC_NULL, ch, fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: bad minute.\n", __FILE__, __LINE__);
    goto eof;
  }

  // hour
  ch = get_list(e->hour, FIRST_HOUR, LAST_HOUR, PPC_NULL, ch, fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: bad hour.\n", __FILE__, __LINE__);
    goto eof;
  }

  // DOM (days of month)
  if (ch == '*')
    e->flags |= DOM_STAR;
  ch = get_list(e->dom, FIRST_DOM, LAST_DOM, PPC_NULL, ch, fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: bad day-of-month.\n", __FILE__, __LINE__);
    goto eof;
  }

  // month 
  ch = get_list(e->month, FIRST_MONTH, LAST_MONTH, MonthNames, ch, fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: bad month.\n", __FILE__, __LINE__);
    goto eof;
  }

  // DOW (days of week)
  if (ch == '*')
    e->flags |= DOW_STAR;
  ch = get_list(e->dow, FIRST_DOW, LAST_DOW, DowNames, ch, fp);
  if (ch == EOF) {
    fprintf(stderr, "%s:%d: Error: bad day-of-week.\n", __FILE__, __LINE__);
    goto eof;
  }

  // make sundays equivilent
  if (bit_test(e->dow, 0) || bit_test(e->dow, 7)) {
    bit_set(e->dow, 0);
    bit_set(e->dow, 7);
  }

  return e;

 eof:
  free_entry(e);
  return NULL;
}


/**
 * @brief crontabフォーマットの文字列を解析して、直近の時刻を取得する。
 * @param[out] result         取得した時刻が反映される。
 * @param[in]  str            解析するcrontabフォーマットの文字列。
 * @param[in]  range_backward 検索する過去の範囲(sec)
 * @param[in]  range_forward  検索する未来の範囲(sec)
 * @return 成功時には0を、失敗時には-1、strの書式が不正な場合は1、時刻が見つからない場合は2を返す。
 */
static int process(time_t *result, const char* str,
		   unsigned int range_backward, unsigned int range_forward)
{
  assert(str != NULL);

  // tmpfileに読み込んだ内容を書き込む。
  FILE *tmpfp = tmpfile();
  if (tmpfp == NULL) {
    fprintf(stderr, "%s:%d: Error: tmpfile().\n", __FILE__, __LINE__);
    return -1;
  }
  fputs(str, tmpfp);
  fputs("\n", tmpfp);
  fflush(tmpfp);
  rewind(tmpfp);

  // 文字列からデータを起こす。
  entry *e = parse_string(tmpfp);
  if (e == NULL) {
    fclose(tmpfp);
    return 1;
  }
  fclose(tmpfp);

  // 時刻を取得
  time_t start = time(NULL) - range_backward;
  time_t range = range_backward + range_forward;
  if (attack(result, e, start, range) != 0) {
    fprintf(stderr, "%s:%d: Error: Not found.\n", __FILE__, __LINE__);
    free_entry(e);
    return 2;
  }

  free_entry(e);

  return 0;
}


/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm crontab"
    " [-r range_backward] [-R range_forward] [-v] [-h] schedule\n";

  const char *description = ""
    "引数から取得したcrontab形式の文字列を解析して、直近の時刻を取得します。"
"取得した時刻はstdinから読み込んだスケジュールの開始時刻に反映して、stdoutに出力します。\n"
    "\n"
    "デフォルトの検索範囲は、プログラム実行時刻から24時間です。\n";

  const char *posarg = "ARGUMENT\n"
    "\tschedule crontab形式の時刻指定\n";

  const char *optarg = "OPTIONS\n"
    "\t-r range_backward 実行時刻を基準とした時刻を検索する過去の範囲(sec)。\n"
    "\t-R range_forward  実行時刻を基準とした時刻を検索する未来の範囲(sec)。\n"
    "\t-v                verboseモード\n"
    "\t-h                show this help message and exit\n";

  const char *exit_status = "EXIT STATUS\n"
    "\t0 正常終了\n"
    "\t1 異常終了\n"
    "\t2 使用方法に誤りがある場合\n"
    "\t3 指定された時刻が見つからない場合\n";

  const char *example = "EXAMPLE\n"
    "\t2017年8月20日午前7時00分から10分間のスケジュールを作成する。\n"
    "\t$ echo \"0:600:今朝のニュース\" | tm crontab \"0 7 20 8 *\"\n"
    "\t1503180600:600:今朝のニュース\n"
    "\n"
    "\t始めの1行をスケジュールとして読み込み、それ以降はそのまま出力される。\n"
    "\t$ echo -e \"0:600:今朝のニュース\\nABCDEFG\" | tm crontab \"0 7 20 8 *\"\n"
    "\t1503180600:600:今朝のニュース\n"
    "\tABCDEFG\n";
    
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n%s\n",
	  usage, description, posarg, optarg, exit_status, example);
}


/**
 * @brief stdinからスケジュールを読み込む。
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

  //
  char sep0, sep1;
  sscanf(buf, "%ld%c%u%c%[^\n]", &(sched->start), &sep0, &(sched->duration),
	 &sep1, sched->caption);

  // 区切り文字をチェック
  if (sep0 != ':' || sep1 != ':') {
    fprintf(stderr, "%s:%d: Error: Unknown schedule format.\n", __FILE__,
	    __LINE__);
    return 1;
  }

  // 開始時刻がマイナスはあり得ない。
  if (sched->start < 0) {
    fprintf(stderr, "%s:%d: Error: Invalid start value.\n", __FILE__,__LINE__);
    return 1;
  }

  if (verbose > 0) {
    fprintf(stderr, "%s:%d: debug: in start:%ld, duration:%d, caption:%s\n",
	    __FILE__, __LINE__, sched->start, sched->duration, sched->caption);
  }

  return 0;
}


int crontab(int argc, char *argv[])
{
  char *arg = NULL;
  unsigned int range_backward = 0, range_forward = 60*60*24;//24hours

  // オプション解析
  switch (parse_arguments(argc, argv, &arg, &range_backward, &range_forward,
			  &verbose)) {
  case 1:
    return EXIT_SUCCESS;
  case 2:
    return EXIT_MISUSE;
  }

  // stdinからスケジュールを取得する。
  struct schedule sched;
  switch (read_schedule(&sched)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_MISUSE;
  }

  if (verbose > 0) {
    fprintf(stderr, "%s:%d: range_b:%dsec range_f:%dsec arg:%s\n", __FILE__,
	    __LINE__, range_backward, range_forward, arg);
  }

  // 指定された開始時刻取得。
  time_t start = 0;
  switch (process(&start, arg, range_backward, range_forward)) {
  case -1:
    return EXIT_FAILURE;
  case 1:
    return EXIT_MISUSE;
  case 2:
    return EXIT_NOT_FOUND;
  }

  // 取得した開始時刻を適応したスケジュールをstdoutに出力する。
  output_schedule(&sched, start);

  // 残りのstdinの内容をstdoutに受け流す。
  if (output_input() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
