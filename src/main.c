/*
 * main.c
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
 * @file tm.c
 * @brief TimeManagerの実装
 */

/** @mainpage
 *
 * TimeManager(tm)は、任意のプログラムの開始時刻と終了時刻を管理するプログラムです。\n
 * 任意のプログラムとともに実行したり、パイプラインに組み込むことで、
 * プログラムを指定の時刻に実行、終了させることができます。
 *
 * TimeManagerは以下のコマンドから構成されています。\n
 * - set        スケジュールをデータベースに追加、有効化する\n
 * - schedule   データベース内のスケジュールを出力する\n
 * - unoccupied 空き時間のスケジュールを作成する\n
 * - crontab    crontab形式で指定した開始時刻をセットする\n
 * - reset      データベース及びロックを初期化する\n
 * - terminate  自プロセスグループを終了させる
 *
 * 最も基本的な使い方は以下です。setコマンドを使います。\n
 * TimeManagerでは、プロセスグループを基本とします。\n
 * 時刻を管理したいプログラムは、TimeManagerと同じプロセスグループで実行します。
 * \code
 * # Run my program for 60sec at 00:00:00 on 2000/01/01.
 * $ sh -c 'echo "946652400:60:This is my program" | tm set && myprogram'
 *
 * # 上記同スケジュールで音楽を再生する。(setコマンドは開始時刻までパイプラインの内容をせき止める)
 * $ sh -c 'echo "946652400:60:Play music.\nmusic.wav" | tm set | xargs aplay'
 * \endcode
 *
 * 開始、終了時刻はスケジュールとして、プロセスグループごとに管理されます。\n
 * スケジュールは共有され、他のプロセスから参照できます。\n
 *
 * スケジュールはscheduleコマンドで参照できます。\n
 * \code
 * # 人間向け。
 * $ tm schedule
 * 01/01 00:00-00:01 (1m) This is my program
 *
 * # プログラム向け。
 * $ tm schedule -r
 * 946652400:60:This is my program
 * \endcode
 *
 *
 * スケジュールが入っていない、空き時間を見つけることで、\n
 * 他のプログラムの実行時刻を考慮した、プログラムの実行ができるようになります。\n
 * 空き時間のスケジュールは、unoccupiedコマンドで取得することができます。\n
 * \code
 * # (2018年)1月29日午前10時14分34秒から1時間はスケジュールが空いている。
 * $ echo "0:0:caption" | tm unoccupied
 * 1517188474:3600:caption
 * \endcode
 *
 *
 * 特定の開始時刻を指定したい場合は、crontabコマンドを使うと便利です。
 * \code
 * # (2017年)8月20日午前7時00分から10分間のスケジュールを作成する。
 * $ echo "0:600:今朝のニュース" | tm crontab "0 7 20 8 *"
 * 1503180600:600:今朝のニュース
 * \endcode
 *
 *
 * 導入方法\n
 * (installには管理者権限が必要。/usr/local/binにイントールされます。)
 * \code
 * $ git clone https://github.com/ll0s0ll/timemanager.git
 * $ cd timemanager
 * $ make
 * $ make install
 * \endcode
 *
 *
 * Last update 2018/02/22
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/activate.h"
#include "../include/add.h"
#include "../include/autoextend.h"
#include "../include/common.h"
#include "../include/crontab.h"
#include "../include/lock.h"
#include "../include/reset.h"
#include "../include/schedule.h"
#include "../include/set.h"
#include "../include/terminate.h"
#include "../include/unlock.h"
#include "../include/unoccupied.h"


/**
 * @brief ヘルプをstderrに出力する。
 */
static void print_usage()
{
  const char *usage = "usage: tm <command> [<args>] [-h]\n"
    "<command>\n"
    "crontab|reset|schedule|set|terminate|unoccupied\n"
    "See 'tm <command> -h' for more information on a specific command.\n";
  
  const char *description = "任意のプログラムの開始時刻と終了時刻を管理する"
    "プログラムです。\n"
    "任意のプログラムとともに実行したり、パイプラインに組み込むことで、"
    "プログラムを指定の時刻に実行、終了させることができます。\n"
    "\n"
    "TimeManagerでは、プロセスグループを基本とします。\n"
    "時刻を管理したいプログラムは、TimeManagerと同じプロセスグループで"
    "実行します。\n"
    "\n"
    "開始、終了時刻はスケジュールとして、プロセスグループごとに"
    "管理されます。\n"
    "スケジュールは共有され、他のプロセスから参照できますので、\n"
    "他のプログラムの実行時刻を考慮した、プログラムの実行ができるように"
    "なります。\n"
    "\n"
    "COMMAND\n"
    "\tset        スケジュールをデータベースに追加、有効化する\n"
    "\tcrontab    crontab形式で指定した開始時刻をセットする\n"
    "\tunoccupied 空き時間のスケジュールを作成する\n"
    "\treset      データベース及びロックを初期化する\n"
    "\tschedule   データベース内のスケジュールを出力する\n"
    "\tterminate  自プロセスグループを終了させる\n"
    "\n"
    "\tそれぞれのコマンドの詳しい情報は'tm <command> -h'を参照してください。\n";
    
    const char *example = "EXAMPLE\n"
    "\tRun my program for 60sec at 00:00:00 on 2000/01/01.\n"
    "\t$ sh -c 'echo \"946652400:60:This is my program\" | tm set && myprogram'\n";

  
  fprintf(stderr, "%s\n%s\n%s\n", usage, description, example);
}


int main (int argc, char* argv[])
{
  if (argc == 1 || strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0) {
    print_usage();
    return EXIT_SUCCESS;
  }

  if (strcmp(argv[1], "activate") == 0) {

    return activate(argc, argv);

  } else if (strcmp(argv[1], "autoextend") == 0) {

    return autoextend(argc, argv);

  } else if (strcmp(argv[1], "add") == 0) {

    return add(argc, argv);

  } else if (strcmp(argv[1], "crontab") == 0) {

    return crontab(argc, argv);

  } else if (strcmp(argv[1], "unlock") == 0) {

    return  unlock(argc, argv);

  } else if (strcmp(argv[1], "lock") == 0) {

    return lock(argc, argv);

  } else if (strcmp(argv[1], "reset") == 0) {

    return reset(argc, argv);

  } else if (strcmp(argv[1], "schedule") == 0) {

    return schedule(argc, argv);

  } else if (strcmp(argv[1], "unoccupied") == 0) {

    return unoccupied(argc, argv);

  } else if (strcmp(argv[1], "terminate") == 0) {

    return terminate(argc, argv);

  } else if (strcmp(argv[1], "set") == 0) {

    return set(argc, argv);

  } else {
    fprintf(stderr, "%s: Error: Unknown command. \'%s\'\n", __FILE__, argv[1]);
    return EXIT_MISUSE;
  }

  fprintf(stderr, "%s:%d: Error: Unknown error.\n", __FILE__, __LINE__);
  return EXIT_FAILURE;
}
