# TimeManager
TimeManager[tm]は、任意のプログラムの開始時刻と終了時刻を管理するプログラムです。
任意のプログラムとともに実行したり、パイプラインに組み込むことで、 
プログラムを指定の時刻に実行、終了させることができます。

TimeManagerは以下のコマンドから構成されています。
- set スケジュールをデータベースに追加、有効化する
- schedule データベース内のスケジュールを出力する
- unoccupied 空き時間のスケジュールを作成する
- crontab crontab形式で指定した開始時刻をセットする
- reset データベース及びロックを初期化する
- terminate 自プロセスグループを終了させる

最も基本的な使い方は以下です。setコマンドを使います。
TimeManagerでは、プロセスグループを基本とします。
時刻を管理したいプログラムは、TimeManagerと同じプロセスグループで実行します。
```
# Run my program for 60sec at 00:00:00 on 2000/01/01.
$ sh -c 'echo "946652400:60:This is my program" | tm set && myprogram'

# 上記同スケジュールで音楽を再生する。(setコマンドは開始時刻までパイプラインの内容をせき止める)
$ sh -c 'echo "946652400:60:Play music.\nmusic.wav" | tm set | xargs aplay'
```

開始、終了時刻はスケジュールとして、プロセスグループごとに管理されます。
スケジュールは共有され、他のプロセスから参照できます。
スケジュールはscheduleコマンドで参照できます。
```
# 人間向け。
$ tm schedule
01/01 00:00-00:01 (1m) This is my program

# プログラム向け。
$ tm schedule -r
946652400:60:This is my program
```
スケジュールが入っていない、空き時間を見つけることで、
他のプログラムの実行時刻を考慮した、プログラムの実行ができるようになります。
空き時間のスケジュールは、unoccupiedコマンドで取得することができます。
```
# (2018年)1月29日午前10時14分34秒から1時間はスケジュールが空いている。
$ echo "0:0:caption" | tm unoccupied
1517188474:3600:caption
```

特定の開始時刻を指定したい場合は、crontabコマンドを使うと便利です。
```
# (2017年)8月20日午前7時00分から10分間のスケジュールを作成する。
$ echo "0:600:今朝のニュース" | tm crontab "0 7 20 8 *"
1503180600:600:今朝のニュース
```

導入方法
(installには管理者権限が必要。/usr/local/binにイントールされます。)
```
$ git clone https://github.com/ll0s0ll/timemanager.git
$ cd timemanager
$ make
$ make install
```

Doxygen
https://ll0s0ll.github.io/timemanager/

Last update 2018/02/22