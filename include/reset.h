/**
 * @file reset.h
 * @brief データベース及びロックの初期化に関する宣言と説明。
 *
 * データベースとして使用している共有メモリ、ロックに使用しているセマフォを、
 * それぞれアンリンクします。
 */
#ifndef _RESET_H_
#define _RESET_H_

/**
 * @brief 共有メモリ、セマフォをアンリンクする。
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2を返す。
 */
int reset(int argc, char* argv[]);

#endif
