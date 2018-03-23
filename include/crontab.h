#ifndef _CRONTAB_H_
#define _CRONTAB_H_

/**
 * @file crontab.h
 * @brief crontab形式で指定した開始時刻を取得するコマンドに関する宣言。
 */

/**
 * @brief crontab形式で指定した開始時刻を取得する。
 *
 * 引数から取得したcrontab形式の文字列を解析して、直近の時刻を取得する。
 * 取得した時刻はstdinから読み込んだスケジュールの開始時刻に反映して、
 * stdoutに出力する。
 *
 * - デフォルトの検索範囲は、プログラム実行時刻から24時間。
 *
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2、時刻が見つから
 * ない場合は3を返す。
 */
int crontab(int argc, char *argv[]);

#endif
