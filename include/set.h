/**
 * @file set.h
 * @brief スケジュールをデータベースに追加、有効化するコマンドに関する宣言。
 *
 * 内部ではadd、activateコマンドを順番に実行している。
 * \sa add.h activate.h
 */

#ifndef _SET_H_
#define _SET_H_

/**
 * @brief stdinからスケジュールを読み込み、有効化する。
 *
 * 内部でadd、activateコマンドを順番に実行している。
 * 
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int set(int argc, char *argv[]);

#endif
