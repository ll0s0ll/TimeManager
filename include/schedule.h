#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

/**
 * @file schedule.h
 * @brief データベースのスケジュールを出力するコマンドに関する宣言。
 */

/**
 * @brief データベースにある有効なスケジュールをstdoutに出力します。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int schedule(int argc, char* argv[]);

#endif
