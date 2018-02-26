/*
 * activate.c
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
 * @file activate.c
 * @brief スケジュールを有効にするコマンドに関する実装。
 */

#include "../include/activate.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"
#include "../include/lock.h"
#include "../include/unlock.h"

#define DEFAULT_SIGNO SIGTERM

// For MacOS X
#if defined(__MACH__) && !defined(CLOCK_REALTIME)
#include <sys/time.h>
#define CLOCK_REALTIME 0
#endif

static int g_argc;
static char* *g_argv;
static int verbose = 0;

static void termination_handler(int sig, siginfo_t *siginfo, void *ucontext);

/**
 * @brief stdin、stdoutを閉じる。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int close_unused_pipes()
{
  errno = 0;
  if (close(STDIN_FILENO) != 0) {
    fprintf(stderr, "%s:%d: Bug!: close() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  if (close(STDOUT_FILENO) != 0) {
    fprintf(stderr, "%s:%d: Bug!: close() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }
  
  return 0;
}


#if defined(__MACH__)
// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_REALTIME will have already been defined.
static int clock_gettime(int clk_id, struct timespec* t)
{
  // c - clock_gettime alternative in Mac OS X - Stack Overflow
  // https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
  struct timeval now;
  int rv = gettimeofday(&now, NULL);
  if (rv) return rv;
  t->tv_sec  = now.tv_sec;
  t->tv_nsec = now.tv_usec * 1000;
  return 0;
}
#endif


/**
 * @brief stdinから受け取ったデータをstdoutに出力する。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int pass_another_data_from_stdin_to_stdout()
{
  while (1) {
    char buf[BUFSIZ];

    size_t num = fread(buf, sizeof(char), BUFSIZ, stdin);
    if (num == 0)
      break;

    size_t num_write = fwrite(buf, sizeof(char), num, stdout);
    fflush(stdout);

    if (num < BUFSIZ) {
      // エラー
      if (ferror(stdin)) {
	fprintf(stderr, "%s:%d: Error: %s\n",
		__FILE__, __LINE__, strerror(errno));
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
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "tm activate [-d <database>] [-s <signo>] [-v] [-h]\n";
  const char *description = "データベースにある自プロセスグループの"
    "スケジュールを有効にします。\n"
    "\n"
    "正常に有効化ができると、開始時刻までブロックし、開始時刻とともにstdinの"
    "内容をそのままstdoutに受け流し、終了します。\n"
    "また、終了時刻には、自プロセスグループに指定のシグナルを送信します。"
    "送信されるシグナルのデフォルトはSIGTERMです。\n"
    "\n"
    "開始時刻後に再度実行された場合は、終了時刻が再スケジュールされます。\n";

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
  
  fprintf(stderr, "usage: %s\n%s\n%s\n%s\n%s\n",
	  usage, description, optarg, exit_status, env);
}


/**
 * @brief コマンドライン引数を解析する。
 * @param[in]  argc     argc値
 * @param[in]  argv     argv値
 * @param[out] shm_name '-d'オプション(データベース番号)が反映される。
 * @param[out] d_opt    '-d'オプション(データベース番号)が指定された場合、1が設定される。
 * @param[out] signo    '-s'オプション(終了時刻に送信されるシグナルの番号)の値が反映される。
 * @param[out] verbose  '-v'オプション(verboseモード)の値が反映される。
 * @return 成功時は0、'h'オプションが指定された場合は1、不正な値が与えられた場
 * 合は2を返す。
 */
static int parse_arguments(int argc, char* *argv, char *shm_name, int *d_opt,
			   int *signo, int *verbose)
{  
  // TimeManagerから呼ばれる場合、argvは{"tm", "activate", "opt"...}となる。
  // オプションを読み込むためには、optindを1つ進めて2にしておく必要がある。
  opterr = 0;
  optind = 2;
  int opt;
  while ((opt = getopt(argc, argv, "d:hs:v")) != -1) {
    switch (opt) {
    case 'd':
      // データベース番号
      if (atoi(optarg) < 1 || atoi(optarg) > MAX_NUM_DB) {
	fprintf(stderr, "Error: Invalid database number. (1-%d)\n",MAX_NUM_DB);
	return 2;
      }
      strcat(shm_name, optarg);
      *d_opt = 1;
      break;
    case 'h':
      // ヘルプ
      print_usage();
      return 1;
    case 's':
      // 終了時刻に送信されるシグナルの番号
      *signo = atoi(optarg);
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
 * @brief SIGTERM,SIGINT,SIGQUITのシグナルハンドラをデフォルトに設定する。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int reset_signal_handler()
{
  struct sigaction sa_dfl;
  sa_dfl.sa_handler = SIG_DFL;
  sa_dfl.sa_flags = 0;
  sigemptyset(&sa_dfl.sa_mask);
      
  errno = 0;
  if (sigaction(SIGTERM, &sa_dfl, NULL) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  if (sigaction(SIGINT, &sa_dfl, NULL) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  if (sigaction(SIGQUIT, &sa_dfl, NULL) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  return 0;
}


/**
 * @brief SIGTERM,SIGINT,SIGQUITのシグナルハンドラを設定する。
 * @return 成功時は0、失敗時には-1を返す。
 */
static int setup_signal_handler()
{
  struct sigaction sa, sa_org_term, sa_org_quit, sa_org_int;
  sa.sa_sigaction = termination_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  errno = 0;
  if (sigaction(SIGTERM, &sa, &sa_org_term) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }
  if (sigaction(SIGINT, &sa, &sa_org_int) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }
  if (sigaction(SIGQUIT, &sa, &sa_org_quit) != 0) {
    fprintf(stderr, "%s:%d: Bug!: sigaction() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

return 0;
}

/**
 * @brief 開始時刻までの待ち時間中にシグナルで終了する場合の対策。
 */
static void termination_handler(int sig, siginfo_t *siginfo, void *ucontext)
{
  //fprintf(stderr, "%s:%d:%s:%d: DEBUG: termination_handler signal:%d\n",
  //__FILE__,__LINE__, strtok(ctime(&t), "\n\0"), getpid(), siginfo->si_signo);

  // エラーは無視。
  unlock(g_argc, g_argv);

  _exit(128 + siginfo->si_signo);
}


/**
 * @brief Calculate diff of two struct timespec
 * @param[in] start
 * @param[in] stop
 * @param[out] result
 */
static void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
  // Calculate diff of two struct timespec · GitHub
  // https://gist.github.com/diabloneo/9619917
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }
  return;
}


/**
 * @brief startで指定された時刻までブロックする。
 * @param[in] start 指定時刻(time_t)
 * @param[in] is_end verboseモード用。0はstart、1はend。
 * @param[out] 成功時0、失敗時-1。
 */
static int wait_till_the_time(time_t time, int is_end)
{
  char mark[] = "start";
  if (is_end) {
    strcpy(mark, "end");
  }

  struct timespec ts_current;
  if (clock_gettime(CLOCK_REALTIME, &ts_current) != 0) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return -1;
  }

  struct timespec ts_start;
  ts_start.tv_sec = time;
  ts_start.tv_nsec = 0;
      
  struct timespec ts_interval;
  timespec_diff(&ts_current, &ts_start, &ts_interval);
  if (verbose > 0)
    fprintf(stderr, "%s:%d: DEBUG: %s interval_sec:%ld interval_nsec:%ld\n",
	    __FILE__, __LINE__, mark, ts_interval.tv_sec, ts_interval.tv_nsec);

  if (ts_interval.tv_sec < 0)
    return 0;

  errno = 0;
  if (nanosleep(&ts_interval, NULL) != 0) {
    fprintf(stderr, "%s:%d: Bug!: nanosleep() %s\n" ,__FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  return 0;
}


int activate(int argc, char *argv[])
{
  char shm_name[NAME_MAX] = DEFAULT_SHARED_MEMORY_NAME;
  int signo = DEFAULT_SIGNO, opt_d = 0;

  // シグナルハンドラ用。
  g_argc = argc;
  g_argv = argv;

  // オプション解析
  switch (parse_arguments(argc, argv, shm_name, &opt_d, &signo, &verbose)) {
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
    fprintf(stderr, "%s:%d: shm_name:%s signo:%d\n", __FILE__, __LINE__,
	    shm_name, signo);
  }

  // シグナルハンドラを設定する。
  if (setup_signal_handler() != 0)
    return EXIT_FAILURE;

  // 子プロセスのゾンビ化回避。
  struct sigaction sa_chld;
  sa_chld.sa_handler = SIG_IGN;
  sa_chld.sa_flags = SA_NOCLDWAIT;
  errno = 0;
  if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return EXIT_FAILURE;
  }

  // データベースをロックする。
  if (lock(argc, argv) != 0)
    return EXIT_FAILURE;

  // データベースからスケジュールを取得する。
  struct schedule* scheds[MAX_NUM_SCHEDULES];
  size_t scheds_len = 0;
  if (load_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, MAX_NUM_SCHEDULES,
		     &scheds_len) != 0) {
    unlock(argc, argv);
    return EXIT_FAILURE;
  }
  
  // 取得したスケジュールから自プロセスグループのスケジュールを取得する。
  struct schedule *s = NULL;
  if (find_sched_by_pgid(getpgid(0), scheds, scheds_len, &s) != 0) {
    fprintf(stderr, "%s:%d: Error: Could not found schedule for pgid %d.\n",
	    __FILE__, __LINE__, getpgid(0));
    cleanup_schedules(scheds, scheds_len);
    unlock(argc, argv);
    return EXIT_MISUSE;
  }

  if (verbose > 0) {
    fprintf(stderr,
       "%s:%d: DEBUG: pgid:%d lock:%d terminator:%d start:%ld dur:%d cap:%s\n",
	    __FILE__, __LINE__, s->pgid, s->lock, s->terminator, s->start,
	    s->duration,s->caption);
  }

  // 上書きの場合は、既存プロセスをkillする。
  if (s->terminator != 0) {
    if (verbose > 0) {
      fprintf(stderr, "%s:%d: DEBUG: Second activation. %d.\n", __FILE__,
	      __LINE__, getpgid(0));
    }

    errno = 0;
    if (kill(s->terminator, SIGTERM) == -1) {
	fprintf(stderr, "%s:%d: Error: %s. to:%d, sig:%d\n",
		__FILE__, __LINE__, strerror(errno), getpgid(0), SIGTERM);
	unlock(argc, argv);
	return EXIT_FAILURE;
    }
  }

  // 終了機能
  errno = 0;
  pid_t child_pid;
  switch (child_pid = fork()) {
  case -1:
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    cleanup_schedules(scheds, scheds_len);
    unlock(argc, argv);
    return EXIT_FAILURE;
  case 0:
    {
      // 子プロセス
      // TimeManagerに登録したプロセスグループを終了させる役割。
      // 終了時刻まで待ち、終了時刻になったら、自プロセスグループにシグナルを
      // 送信する。親プロセスにwaitされず、initに引き取られる。

      if (verbose > 0) {
	fprintf(stderr, "%s:%d: child pid:%d pgid:%d\n", __FILE__, __LINE__,
		getpid(), getpgid(0));
      }

      // 不要なパイプを閉じる。
      if (close_unused_pipes() != 0)
	_exit(1);

      // 親プロセスで変更されたシグナルハンドラをデフォルト値に戻す。
      if (reset_signal_handler() != 0)
	_exit(1);

      // 必要な値のみ取り出して、親から受け継いだものを掃除する。
      time_t end = s->start + s->duration;
      cleanup_schedules(scheds, scheds_len);

      // 終了時刻まで待つ。
      if (wait_till_the_time(end, 1) != 0)
	_exit(1);	

      // 自プロセスグループにシグナルを送信。
      errno = 0;
      if (killpg(getpgid(0), signo) == -1) {
	fprintf(stderr, "%s:%d: Bug!: killpg() %s. to:%d, sig:%d\n", __FILE__,
		__LINE__, strerror(errno), getpgid(0), signo);
	_exit(1);
      }

      _exit(0);
    }
  default:
    {
      // 親プロセス
      // 開始時刻まで待ち、開始時刻になったらstdinの内容をstdoutに受け流して、
      // 終了する。子プロセスの面倒は見ない。

      if (verbose > 0) {
	fprintf(stderr, "%s:%d: parent pid:%d pgid:%d\n", __FILE__, __LINE__,
		getpid(), getpgid(0));
      }

      // 申し訳ないが、子プロセスはinitに引き取ってもらう。//

      // 子プロセスのpidを保存する。
      s->terminator = child_pid;

      // データベースを更新する。
      if (save_schedules(shm_name, SHARED_MEMORY_SIZE, scheds, scheds_len)!=0){
	cleanup_schedules(scheds, scheds_len);
	unlock(argc, argv);
	return EXIT_FAILURE;
      }
   
      // データベースのロックを解放する。
      if (unlock(argc, argv) != 0)
	return EXIT_FAILURE;

      // 必要な値のみ取り出して、掃除する。
      time_t start = s->start;
      cleanup_schedules(scheds, scheds_len);

      // アクティベート処理ここまで //
       
      // 開始時刻まで待つ。
      if (wait_till_the_time(start, 0) != 0)
	return EXIT_FAILURE;

      // 残りのstdinの内容をstdoutに受け流す。
      if (pass_another_data_from_stdin_to_stdout() != 0)
	return EXIT_FAILURE;

    } // default
  } // switch
  
  return EXIT_SUCCESS;
}
