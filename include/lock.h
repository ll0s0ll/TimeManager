/**
 * @file lock.h
 * @brief データベース書き換えロックに関する宣言と説明。
 *
 * semaphoreを使用して複数プロセス間のデータベースへの書き込みの同期を取る。\n
 * lock関数ではsemaphoreを獲得し、unlock関数で解放する。\n
 * semaphore獲得状況は、自プロセスグループのスケジュールのlock値に反映される。
 */
#ifndef _LOCK_H_
#define _LOCK_H_

/**
 * @brief スケジュールの書き換えをロックする。
 *
 * 他のプロセスによってロックされている場合は、ロックが解除されるまで待つ。
 * 指定時間以内にロックが解除されない場合は、タイムアウトする。
 *
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2、タイムアウトした場合は3を返す。
 */
int lock(int argc, char* argv[]);

#endif
