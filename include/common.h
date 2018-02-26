/**
 * @file common.h
 * @brief TimeManagerの共通部分に関する宣言と説明
 *
 * - データベース\n
 * データベースは、各プロセスグループのスケジュールを1レコードとして記録した
 * もので、共有メモリ上に記録される。\n
 * スケジュールは、schedule構造体の内容を文字列で表したもので、\n
 * pgid,lock,terminator,start,duration,captionの順に、値をコロン(:)でつなげた書式である。\n
 * 記録するスケジュール数の上限は、MAX_NUM_SCHEDULES値で指定される。\n
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <sys/types.h>

/**
 * @def DEFAULT_SCHED_CAPTION
 * @brief スケジュールのcaptionの初期値
 */
#define DEFAULT_SCHED_CAPTION "TimeManager."

/**
 * @def DEFAULT_SEMAPHORE_NAME
 * @brief セマフォのパスのデフォルト値。
 */
#define DEFAULT_SEMAPHORE_NAME "/sem_timemanager"

/**
 * @def DEFAULT_SHARED_MEMORY_NAME
 * @brief 共有メモリのパスのデフォルト値
 */
#define DEFAULT_SHARED_MEMORY_NAME "/shm_timemanager"

/**
 * @def SHARED_MEMORY_SIZE
 * @brief 共有メモリのサイズ
 */
#define SHARED_MEMORY_SIZE 65536

/**
 * @def MAX_NUM_DB
 * @brief データベースの最大数
 */
#define MAX_NUM_DB 5

/**
 * @def ENV_NAME
 * @brief データベースを指定する環境変数名
 */
#define ENV_NAME "TM_DB_NUM"

/**
 * @def EXIT_MISUSE
 * @brief 誤った使い方の場合の終了ステータス
 */
#define EXIT_MISUSE 2

/**
 * @def MAX_NUM_SCHEDULES
 * @brief 読み込むスケジュールの最大数。
 */
#define MAX_NUM_SCHEDULES 1024

/**
 * @def MAX_CAPTION_LEN
 * @brief schedule構造体のcaptionの最大文字数(英数時、終端文字列含む。)
 */
#define MAX_CAPTION_LEN 256

/**
 * @def MAX_SCHEDULE_STRING_LEN
 * @brief スケジュールを表す文字列の最大文字数(英数時、終端文字列含む。)
 */
#define MAX_SCHEDULE_STRING_LEN 512

/**
 * @def MAX_RECORD_STRING_LEN
 * @brief 共有メモリに保存される、スケジュールの内容を含んだレコードの最大文字数。
 * (半角英数時。末尾の改行、終端文字列は含まない。)
 */
#define MAX_RECORD_STRING_LEN 510

/**
 * @struct schedule
 * @brief スケジュールに関する情報を保持する構造体
 */
struct schedule {
  pid_t pgid;  /**< 実行されるプロセスが属するプロセスグループID。*/
  int lock; /**< ロック確保状態(0:未確保 1:確保中) */
  pid_t terminator; /**< 終了時刻を通知するプロセスのpid */
  time_t start;  /**< 開始時刻 */
  unsigned int duration;  /**< 継続時間(sec) */
  char caption[MAX_CAPTION_LEN];  /**< スケジュール内容の簡単な説明(改行混入不可)*/
};

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * @brief スケジュールが、スケジュール群の中のスケジュールと重複していないか確認する。
   * @param[in] sched 重複を確認するスケジュール 
   * @param[in] scheds 確認される側のスケジュール群
   * @param[in] len scheds配列の個数
   * @return 重複がない場合は0を、重複がある場合は1を返す。
   */
  int check_sched_conflict(struct schedule* sched, struct schedule* *scheds, size_t len);
  
  /**
   * @brief スケジュール構造体群のメモリをそれぞれ解放する。
   * @param[in] scheds 解放するメモリへのポインタを持つ配列。
   * @param[in] len schedsの配列数。
   */
  void cleanup_schedules(struct schedule* *scheds, size_t len);

  /**
   * @brief 引数を元にスケジュール構造体を作成する。
   * @attention 戻り値のスケジュール構造体は、メモリを動的に確保しているので、
   * 不要時にはメモリの解放をする必要がある。
   * @param[in]  pgid       実行するプロセスが属するプロセスグループID
   * @param[in]  lock       ロックの取得状況(0 or 1)
   * @param[in]  terminator 終了時刻にシグナルを送信するプロセスのID
   * @param[in]  start      開始時刻
   * @param[in]  duration   継続時間(sec)
   * @param[in]  caption    スケジュールの簡単な説明。
   * @param[out] sched      作成したschedule構造体を示すポインタ
   * @pre captionの文字数は、英数時で、@link MAX_CAPTION_LEN-1 @endlink 以下である必要がある。
   * @return 成功時は0、失敗時には-1を返す。
   */
  int create_schedule(pid_t pgid, int lock, pid_t terminator,
		      time_t start, unsigned int duration, const char *caption,
		      struct schedule* *sched);

  /**
   * @brief スケジュール群の内容を、コメントとともにstderrに出力する。
   * @details 書式:comment scheds[x] pgid:xxxx start:xxxxxx dur:xxx cap:xxxxx
   * @param[in] commnet コメント
   * @param[in] scheds 書き出すスケジュール群。
   * @param[in] len schedsの配列数。
   */
  void debug_schedule(const char* comment, struct schedule* *scheds,
		      size_t len);

  /**
   * @brief 環境変数を解析する。
   * @param[out] sem_name セマフォ名。環境変数(データベース番号)が反映される。
   * @param[out] shm_name 共有メモリ名。環境変数(データベース番号)が反映される。
   * @return 成功時は0、失敗時には-1を返す。
   */
  int get_env(char *sem_name, char *shm_name);

  /**
   * @brief 与えられたスケジュール群から、指定されたpgid値を持つスケジュールを見つける。
   * @attention 同じpgid値を持つスケジュールが複数ある場合の動作は考慮していない。
   * @param[in]  pgid   見つけるスケジュールのpgid値
   * @param[in]  scheds 対象のスケジュール群
   * @param[in]  len    schedsの配列数
   * @param[out] sched  見つかったスケジュール構造体が反映される。
   * @return 見つかった場合0、見つからない場合-1。
   */
  int find_sched_by_pgid(pid_t pgid, struct schedule* *scheds, size_t len,
			 struct schedule* *sched);


  /**
   * @brief 与えられたスケジュール群の中から、空き時間のスケジュール群を作成する。
   * @param[in] scheds  対象となるスケジュール群
   * @param[in] len     schedsの配列数
   * @param[out] unoccupied_scheds 作成した空きスケジュール群を保存する配列。
   * あらかじめメモリを確保しておく必要がある。
   * @param[in] max_len     unoccupied_schedsの配列数。
   * @param[in] range_start 空き時間を検索する開始時刻。
   * @param[in] range_dur 空き時間検索範囲。(sec)
   * @param[in] caption 作成した空きスケジュール群のcaption値にセットされる値。
   * @return 作成した空きスケジュールの数。
   */
  size_t generate_unoccupied_scheds_from_scheds(struct schedule** scheds,
						size_t len,
					   struct schedule** unoccupied_scheds,
						size_t max_len,
						time_t range_start,
						unsigned int range_dur,
						const char* caption);

  /**
   * @brief 共有メモリからスケジュールを読込、スケジュール構造体を作成する。
   * @param[in]  shm_path   共有メモリのパス。
   * @param[in]  shm_size   共有メモリのサイズ。
   * @param[out] scheds     読み込んだスケジュール構造体を保存する配列。
   * あらかじめメモリを確保しておく必要がある。
   * @param[in]  scheds_len schedsの配列数。
   * @param[out] loaded_len 読み込んだスケジュール数が反映される。
   * @return 成功時は0、失敗時は-1返す。
   */
  int load_schedules(const char* shm_path, size_t shm_size,
		     struct schedule** scheds, size_t scheds_len,
		     size_t *loaded_len);

  /**
   * @brief スケジュール群を決められた書式で共有メモリに書き込む。
   * @param[in] path 共有メモリのパス。
   * @param[in] size 共有メモリのサイズ。
   * @param[in] scheds 書き込むスケジュール構造体の配列。
   * @param[in] len schedsの配列数。
   * @return 成功した場合は0を、失敗した場合は-1を返す。
   */
  int save_schedules(const char* path, const size_t size,
		     struct schedule** scheds, size_t len);

  /**
   * @brief schedule構造体のstart値で昇順ソートする。
   * @param[in/out] scheds ソートするスケジュール構造体の配列。結果は直接反映される。
   * @param[in] 　　len    schedsの配列数。
   */
  void sort_schedules(struct schedule** scheds, size_t len);

  /**
   * @brief 文字列の内容からスケジュール構造体を作成する。
   * @attention 戻り値のスケジュール構造体は、メモリを動的に確保しているので、
   * 不要時にはメモリの解放をする必要がある。
   * @param[in] str スケジュールを表す文字列。
   * @param[out] sched 作成したschedule構造体を示すポインタ。
   * @return 成功した場合は0を、失敗した場合は-1を返す。
   */
  int string_to_schedule(const char* str, struct schedule* *sched);

#ifdef __cplusplus
}
#endif

#endif
