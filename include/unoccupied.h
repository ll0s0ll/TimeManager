/**
 * @file unoccupied.h
 * @brief 空き時間のスケジュールを作成するコマンドに関する宣言。
 */
#ifndef _UNOCCUPIED_H_
#define _UNOCCUPIED_H_

/**
 * @brief 空き時間のスケジュールを作成し、stdinから読み込んだスケジュールに反映
 * して、stdoutに出力する。
 * 
 * - 入力されたスケジュールの継続時間が、作成した空き時間のスケジュールの継続時
 * 間より大きい場合は、3を返す。
 * - 入力されたスケジュールの継続時間が0以外の場合は、作成した空き時間のスケジ
 * ュールの継続時間を反映しない。
 * - デフォルトの検索開始時刻は、プログラムが実行された時刻。
 * - デフォルトの検索範囲は3600秒。
 *
 * @param[in] argc argc値
 * @param[in] argv argv値
 * @return 成功時は0、失敗時には1、使用方法に誤りがある場合は2、空き時間が見つ
 * からない場合は3を返す。
 */
int unoccupied(int argc, char* argv[]);

#endif
