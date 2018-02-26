#ifndef _ADD_H_
#define _ADD_H_

/**
 * @file add.h
 * @brief データベースにスケジュールを追加するコマンドに関する宣言。
 */

/**
 * @brief stdinからスケジュールを読み込み、データベースにレコードを追加する。
 *
 * stdinから始めの1行をスケジュールとして読み込む。
 * 読み込んだスケジュールの終了時刻が、現在時刻よりも過去の場合は2を返す。
 * 
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int add(int argc, char *argv[]);

#endif
