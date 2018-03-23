/*
 * common.c
 * This file is part of TimeManager.
 *
 * Copyright (C) 2018  Shun ITO <shunito.s110@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file common.c
 * @brief TimeManagerの共通部分に関する実装
 */

#include "../include/common.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h> // for O_WRONLY..etc
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief ファイルの存在を確認する。ファイルが存在しない場合は作成する。
 * @param[in] path スケジュールデータベースのパス
 * @return 成功時0、失敗時-1。
 */
static int check_sched_db_file(const char* path)
{
  errno = 0;
  int fd = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
  if (fd == -1) {
    fprintf(stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__, strerror(errno));
    return -1;
  }
  close(fd);

  return 0;
}


int check_sched_conflict(struct schedule* sched, struct schedule* *scheds,
			 size_t len)
{
  assert(sched != NULL && scheds != NULL);

  int i;
  for (i=0; i<len; i++) {

    // もちろん自分のスケジュールは飛ばす。
    if (scheds[i]->pgid == sched->pgid)
      continue;

    if (scheds[i]->start < (sched->start + sched->duration) &&
	(scheds[i]->start + scheds[i]->duration) > sched->start) {
      /*fprintf(stderr, "Schedule conflict. pgid:%d start:%ld dur:%d cap:%s\n",
	sched->pgid, sched->start, sched->duration, sched->caption);*/
      return 1;
    }
  }

  return 0;
}


void cleanup_schedules(struct schedule* *scheds, size_t len)
{
  assert(scheds != NULL);

  int i;
  for (i=0; i<len; i++) {
    free(scheds[i]);
  }
}


/**
 * @brief qsort()用の関数。スケジュール構造体のstart値で昇順ソートする。
 */
static int compare_start_val(const void *a, const void *b)
{
  //fprintf(stderr, "a:%d, b:%d\n", (*(struct schedule**)a)->pgid, (*(struct schedule**)b)->pgid);

  // 降順
  //return (*(struct schedule**)b)->start - (*(struct schedule**)a)->start;

  // 昇順
  return (*(struct schedule**)a)->start - (*(struct schedule**)b)->start;
}


int create_schedule(pid_t pgid, int lock, pid_t terminator, time_t start,
		    unsigned int duration, const char *caption,
		    struct schedule* *sched)
{
  assert(caption != NULL && strlen(caption) < MAX_CAPTION_LEN);

  *sched = (struct schedule*)malloc(sizeof(struct schedule));
  if (*sched == NULL) {
    fprintf(stderr, "%s:%d: Error: Faild to allocate memory.\n", __FILE__,
	    __LINE__);
    return -1;
  }

  (*sched)->pgid       = pgid;
  (*sched)->lock       = lock;
  (*sched)->terminator = terminator;
  (*sched)->start      = start;
  (*sched)->duration   = duration;
  strcpy((*sched)->caption, caption);

  return 0;
}


void debug_schedule(const char* comment, struct schedule* *scheds, size_t len)
{
  assert(comment != NULL && scheds != NULL);

  if (scheds == NULL) {
    fprintf(stderr, "%s, No schedules.\n", comment);
    return;
  }

  int i;
  for (i=0; i<len; i++) {
    fprintf(stderr,
	    "%s scheds[%d] pgid:%d lock:%d terminator:%d start:%ld dur:%d cap:%s\n",
	    comment, i, scheds[i]->pgid, scheds[i]->lock,scheds[i]->terminator,
	    scheds[i]->start, scheds[i]->duration, scheds[i]->caption); 
  }

  return;
}


/**
 * @brief 環境変数を解析する。
 * @param[out] sem_name セマフォ名。環境変数(データベース番号)が反映される。
 * @param[out] shm_name 共有メモリ名。環境変数(データベース番号)が反映される。
 * @return 成功時は0、失敗時には-1を返す。
 */
int get_env(char *sem_name, char *shm_name)
{
  // 環境変数よりシェルのパスを取得
  char *str = getenv(ENV_NAME);
  if (str != NULL) {
    if (atoi(str) < 1 || atoi(str) > MAX_NUM_DB) {
      fprintf(stderr, "Error: Invalid database number. (Valid 1-%d)\n",
	      MAX_NUM_DB);
      return -1;
    }

    if (sem_name != NULL)
      strcat(sem_name, str);

    if (shm_name != NULL)
      strcat(shm_name, str);
  }

  return 0;
}


int find_sched_by_pgid(pid_t pgid, struct schedule* *scheds, size_t len,
		       struct schedule* *sched)
{
  assert(pgid > 0);
  assert(scheds != NULL);
  
  int i;
  for (i=0; i<len; i++) {
    if (scheds[i]->pgid == pgid) {
      *sched = scheds[i];
      return 0;
    }
  }
  return -1;
}


size_t generate_unoccupied_scheds_from_scheds(struct schedule** scheds,
					      size_t len,
					   struct schedule** unoccupied_scheds,
					      size_t max_len,
					      time_t range_start,
					      unsigned int range_dur,
					      const char* caption)
{
  assert(scheds != NULL && unoccupied_scheds != NULL);
  assert(caption != NULL);

  size_t index_found = 0;
  time_t head = range_start;
  time_t unoccupied_start = 0, unoccupied_end = 0;
  time_t range_end = range_start + range_dur;
  pid_t pgid = getpgid(0);

  // まず、スケジュールをstart値で昇順ソート
  qsort(scheds, len, sizeof(struct schedule*), compare_start_val);

  int i;
  for (i=0; i<len; i++) {
    //fprintf(stderr, "search head:%ld start:%ld end:%ld dur:%d\n", head, scheds[i]->start, scheds[i]->start+scheds[i]->duration, scheds[i]->duration);

    // headがレンジ内か確認する。
    if (head > range_end) {
      //fprintf(stderr, "Out of Range. head:%ld range_end:%ld\n", head, range_end);
      return index_found;
    }

    time_t sched_start = scheds[i]->start;
    time_t sched_end = scheds[i]->start + scheds[i]->duration;

    if (sched_start > head) {
      // ヘッドがスケジュールの開始時間前にある場合。
      //  |<-head
      //  |  +-+-+-+-+-+-+  +-+-+-+-+-+-+-+  
      //  |  | scheds[i] |  | scheds[i+1] | ...
      //  |  +-+-+-+-+-+-+  +-+-+-+-+-+-+-+  
      //  | | <-> |<-range_end 

      // 新しいschedの開始時刻はheadの値になる。
      unoccupied_start = head;

      // 新しいschedの終了時刻は、元のschedの開始時間または検索範囲末尾になる。
      if (sched_start < range_end)
	unoccupied_end = sched_start;
      else
	unoccupied_end = range_end;

    }

    // 新しい空き時間が見つかった場合は、新規作成する。
    if (unoccupied_start != 0 && unoccupied_end != 0) {
      struct schedule *s;
      create_schedule(pgid,
		      0, // lock
		      0, // terminator_pid
		      unoccupied_start,
		      (unoccupied_end-unoccupied_start),
		      caption,
		      &s);

      //fprintf(stderr, "found! %ld, %ld\n", s->start, unoccupied_end);
      if (index_found > max_len-1) {
	fprintf(stderr, "found! but over max size. %zu\n", max_len);
	return index_found;
      }
      unoccupied_scheds[index_found] = s;
      index_found++;
    }

    // ヘッド位置を更新する。
    if (sched_end > range_start)
      head = sched_end;
    else
      head = range_start;

  } //for
  

  if (head < range_end) {
    // schedsの終わりからレンジの終わりまでを調べる。
    //              |<-head |
    //  +-+-+-+-+-+-+       |
    //    scheds[?] |  no more scheds...
    //  +-+-+-+-+-+-+       |
    //              |       |<-range_end 

    struct schedule *s;
    create_schedule(pgid, 0, 0, head, (range_end-head), caption, &s);
    //fprintf(stderr, "found! %ld, %ld\n", s->start, unoccupied_end);
    if (index_found > max_len-1) {
      fprintf(stderr, "found! but over max size. %zu\n", max_len);
      return index_found;
    }
    unoccupied_scheds[index_found] = s;
    index_found++;
  } 

  return index_found;
}


/**
 * @brief 共有メモリのアドレスを取得する。
 * @param[in] path 共有メモリのパス。
 * @param[in] size 共有メモリのサイズ。
 * @param[out] addr 取得したアドレスが反映される。
 * @return 成功した場合は0を、失敗した場合は-1を返す。
 */
static int get_shared_memory_address(const char* path, size_t size,char* *addr)
{
  errno = 0;
  int fd = shm_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    fprintf(stderr, "%s:%d: Error: shm_open() %s\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }
  
  // c - ftruncate not working on POSIX shared memory in Mac OS X - Stack Overflow
  // stackoverflow.com/questions/25502229/ftruncate-not-working-on-posix-shared-memory-in-mac-os-x
  struct stat mapstat;
  if (-1 != fstat(fd, &mapstat) && mapstat.st_size == 0) {
    errno = 0;
    if (ftruncate(fd, size) == -1) {
      fprintf(stderr, "%s:%d: Error: ftruncate. %s\n", __FILE__, __LINE__,
	      strerror(errno));
      return -1;
    }
  }
  
  errno = 0;
  *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    fprintf(stderr, "%s:%d: Error: mmap() %s.\n", __FILE__, __LINE__,
	    strerror(errno));
    return -1;
  }

  if (close(fd) == -1) {
    fprintf(stderr, "%s:%d: Error: close()\n", __FILE__, __LINE__);
    return -1;
  }

  return 0;
}


/**
 * @brief 共有メモリからスケジュールを読み込み、スケジュール構造体を作成する。
 * @param[in]  shm_path 共有メモリのパス。
 * @param[in]  shm_size 共有メモリのサイズ。
 * @param[out] scheds 読み込んだスケジュール構造体を保存する配列。あらかじめ
 * メモリを確保しておく必要がある。
 * @param[in]  scheds_len schedsの配列数。
 * @param[out] loaded_len 読み込んだスケジュール数が反映される。
 * @return 成功時は0、失敗時は-1返す。
 */
int load_schedules(const char* shm_path, size_t shm_size, 
		   struct schedule** scheds, size_t scheds_len,
		   size_t *loaded_len)
{
  assert(scheds_len != 0);

  char *addr;
  if (get_shared_memory_address(shm_path, shm_size, &addr) != 0)
    return -1;

  // strtok()は元の文字列に変更を加えるので、共有メモリの内容をローカルにコピー
  // する。
  char buff[strlen(addr)+1];
  strcpy(buff, addr);

  if (munmap(addr, shm_size) != 0) {
    fprintf(stderr, "%s:%d: Error: munmap()\n", __FILE__, __LINE__);
    return -1;
  }

  char *token;
  token = strtok(buff, "\n");
  if (token == NULL) {
    // buffは空
    return 0;
  }

  //fprintf(stderr, "1st:%s\n", token);

  int index = 0;

  struct schedule* s;
  if (string_to_schedule(token, &s) != 0) {
    return -1;
  }

  // プロセスグループが終了している場合は読み込まない。
  if (killpg(s->pgid, 0) == 0) {
    scheds[index] = s;
    index++;
  } else {
    free(s);
  }
  
  while (1) {
    token = strtok(NULL, "\n");

    if (token == NULL)
      break;

    //fprintf(stderr, "2nd:%s\n", token);

    struct schedule* s;
    if (string_to_schedule(token, &s) != 0) {
      return -1;
    }

    // プロセスグループが終了している場合は読み込まない。
    if (killpg(s->pgid, 0) == 0) {
      scheds[index] = s;
      index++;
    } else {
      free(s);
    }

    if (index+1 >= scheds_len)
      break;
  }

  *loaded_len = index;

  return 0;
}


/**
 * @brief スケジュール群を決められた書式で共有メモリに書き込む。
 * @param[in] path 共有メモリのパス。
 * @param[in] size 共有メモリのサイズ。
 * @param[in] scheds 書き込むスケジュール構造体の配列。
 * @param[in] len schedsの配列数。
 * @return 成功した場合は0を、失敗した場合は-1を返す。
 */
int save_schedules(const char* path, const size_t size,
		   struct schedule** scheds, size_t len)
{
  char *addr;
  if (get_shared_memory_address(path, size, &addr) != 0)
    return -1;
  
  // すべて0で埋めてきれいにする。
  memset(addr, 0x0, size);

  // 共有メモリに書き込むための、各スケジュールをまとめた文字列を作成。
  char sched[size];
  sched[0] = '\0';

  int i;
  for (i=0; i<len; i++) {
    char buff[MAX_RECORD_STRING_LEN+1+1]; // +1は改行分、+1は終端文字列。
    if (sprintf(buff,
		"%d:%d:%d:%ld:%d:%s\n",
		scheds[i]->pgid,
		scheds[i]->lock,
		scheds[i]->terminator,
		scheds[i]->start,
		scheds[i]->duration,
		scheds[i]->caption) < 0) {
      fprintf(stderr, "%s:%d: Error: sprintf()\n", __FILE__, __LINE__);
      return -1;
    }
    strcat(sched, buff);
  }
  //fprintf(stderr, "sched:%s\n", sched);

  // 共有メモリへ書き込み。
  strcpy(addr, sched);

  if (munmap(addr, size) != 0) {
    fprintf(stderr, "%s:%d: Error: munmap()\n", __FILE__, __LINE__);
    return -1;
  }

  return 0;
}


void sort_schedules(struct schedule** scheds, size_t len)
{
  qsort(scheds, len, sizeof(struct schedule*), compare_start_val);
}


int string_to_schedule(const char* str, struct schedule* *sched)
{
  assert(str != NULL);
  //fprintf(stderr, "in:%s\n", str);
  
  int dur;
  int lock;
  pid_t pgid, terminator;
  time_t start;
  char caption[MAX_CAPTION_LEN];
  char sep1, sep2, sep3, sep4, sep5;

  sscanf(str, "%d%c%d%c%d%c%ld%c%d%c%[^\n]",
	 &pgid, &sep1, &lock, &sep2, &terminator, &sep3, &start, &sep4, &dur,
	 &sep5, caption);


  // 区切り文字をチェック
  if (sep1 != ':' || sep2 != ':' || sep3 != ':' || sep4 != ':' || sep5 != ':'){
    fprintf(stderr, "Error: Unknown schedule format. \"%s\"\n", str);
    return -1;
  }

  // lock値は0または1
  if (lock != 0 && lock != 1) {
    fprintf(stderr, "%s:%d: Error: Invalid lock value. lock:%d\n", __FILE__,
	    __LINE__, lock);
    return -1;
  }

  // 開始時刻、継続時間がマイナスはあり得ない。
  if (start < 0 || dur < 0) {
    fprintf(stderr,
	    "%s:%d: Error: Invalid start or dur value. start:%ld dur:%d\n",
	    __FILE__, __LINE__, start, dur);
    return -1;
  }

  //
  if (create_schedule(pgid, lock, terminator, start, dur, caption, sched) != 0)
    return -1;

  return 0;
}
