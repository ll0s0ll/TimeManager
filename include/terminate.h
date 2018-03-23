/**
 * @file terminate.h
 * @brief 自プロセスグループを終了させるコマンドに関する宣言。
 */
#ifndef _TERMINATE_H_
#define _TERMINATE_H_

/**
 * @brief 自分が所属するプロセスグループへSIGTERMを送信して、
 * プロセスグループに所属するプロセスを終了させる。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int terminate(int argc, char* argv[]);

#endif
