/**
 * @file unlock.h
 * @brief データベース書き換えアンロックに関する宣言と説明。
 *
 * semaphoreを使用して複数プロセス間のデータベースへの書き込みの同期を取る。\n
 * lock関数ではsemaphoreを獲得し、unlock関数で解放する。\n
 * semaphore獲得状況は、自プロセスグループのスケジュールのlock値に反映される。
 */
#ifndef _UNLOCK_H_
#define _UNLOCK_H_

/**
 * @brief スケジュールの書き換えをアンロックする。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int unlock(int argc, char* argv[]);

#endif
