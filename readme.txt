xgps-qt
=======

xgps-qt は、gpsd 付属の xgps を Qt 6 ベースで再実装することを目的とした
GPS/GNSS 表示アプリケーションです。

gpsd サーバーへ TCP 接続し、衛星の sky view、TPV の基本測位情報、
衛星一覧を表示します。


動作条件
--------

- Windows
- TCP 接続可能な gpsd サーバー
- gpsd の標準ポート: 2947

現在のデフォルト接続先:

    localhost


Qt Creator でのビルド方法
-------------------------

1. Qt Creator を起動します。
2. ファイル、またはプロジェクトを開く操作で以下のファイルを開きます。

       CMakeLists.txt

3. Kit は以下を選択します。

       Desktop Qt 6.9.2 MinGW 64-bit

4. Configure Project を実行します。
5. Debug または Release 構成を選びます。
6. ビルドを実行します。

ビルドに成功すると、Windows では以下の実行ファイルが作成されます。

    xgps-qt.exe

Qt Creator が作成するビルドフォルダ名は環境によって変わります。
例:

    build\Desktop_Qt_6_9_2_MinGW_64_bit-Debug
    build\Desktop_Qt_6_9_2_MinGW_64_bit-Release

プロジェクトを別フォルダへ移動したあとに CMake エラーが出る場合は、
古い Qt Creator 設定や CMake キャッシュが残っている可能性があります。
その場合は Qt Creator を閉じてから、以下を削除して開き直してください。

    CMakeLists.txt.user
    .qtcreator
    build\Desktop_Qt_6_9_2_MinGW_64_bit-Debug
    build\Desktop_Qt_6_9_2_MinGW_64_bit-Release

特に CMakeCache.txt は作成時のソースパスを保持するため、プロジェクトを
移動した後に使い回すことはできません。


Release パッケージ作成の自動化
------------------------------

Windows 用の Release ビルドと配布フォルダ作成は、以下のスクリプトで
自動化できます。

    scripts\build_release_windows.ps1

PowerShell でプロジェクトフォルダに移動して実行します。

    powershell -ExecutionPolicy Bypass -File .\scripts\build_release_windows.ps1

zip まで作成する場合:

    powershell -ExecutionPolicy Bypass -File .\scripts\build_release_windows.ps1 -Zip

このスクリプトは以下を行います。

1. CMake で Release 構成を作成
2. Ninja でビルド
3. dist\xgps-qt-windows\ を作成
4. xgps-qt.exe をコピー
5. windeployqt、または手動コピーで Qt DLL と platforms\qwindows.dll を配置
6. readme.txt を配布フォルダへコピー
7. -Zip 指定時は xgps-qt-windows.zip を作成

Qt のインストール場所が異なる場合は、引数で変更できます。

    powershell -ExecutionPolicy Bypass -File .\scripts\build_release_windows.ps1 `
      -QtDir C:\Qt\6.9.2\mingw_64 `
      -MingwDir C:\Qt\Tools\mingw1310_64


配布時の注意
------------

Qt Creator で Release ビルドした xgps-qt.exe を他の PC で実行するには、
Qt の DLL と plugin を同梱する必要があります。exe 単体では Qt が入っていない
PC で起動できません。

最低限、配布フォルダには以下を含めてください。

    xgps-qt.exe
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Network.dll
    Qt6Widgets.dll
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    platforms\qwindows.dll

platforms\qwindows.dll は platforms フォルダごと必要です。


ライセンスについて
------------------

Qt を LGPL で利用して配布する場合は、Qt のライセンス表記、LGPL 本文、
Qt ライブラリのソース入手方法など、ライセンス上の条件を確認してください。
商用利用や再配布を行う場合は、Qt のライセンス条件に従ってください。


基本的な使い方
--------------

1. gpsd の Host と Port を入力します。
2. Connect を押します。
3. 接続中はボタン表示が Disconnect に変わります。
4. Disconnect を押すと受信を停止します。

接続ボタンの文字色:

- Connect: 通常色
- Connecting...: 青色
- Disconnect: 赤色
- Cancel Reconnect: 茶色

予期しない切断や接続エラーが発生した場合、最大 5 回まで自動再接続します。
再接続待ち中に Cancel Reconnect を押すと、自動再接続を中止できます。


Sky view の操作
---------------

- マウスホイール: ズームイン / ズームアウト
- 左ドラッグ: sky view の回転
- 右クリック: sky view のコンテキストメニューを表示

右クリックメニュー:

- Show PRN labels: 衛星マーカー横の PRN 番号表示を on/off
- Show Legend: GNSS マーカー凡例を on/off
- Reset View: ズームと回転を初期状態に戻す


衛星マーカー
------------

マーカーの形は GNSS の種類を表します。

- GPS
- SBAS
- Galileo
- BeiDou
- QZSS
- GLONASS
- IRNSS

マーカーの色は信号強度 gpsd の ss を表します。

- ss < 12: 灰色
- ss < 30: 赤
- ss < 36: 黄
- ss < 42: 緑
- ss >= 42: 青緑

塗りつぶしのあるマーカーは、現在の測位に使用されている衛星を表します。
点滅を抑えるため、gpsd が used=false を返しても短時間だけ使用中表示を
保持します。信号強度の色にも、しきい値付近でちらつかないように
小さなヒステリシスを入れています。


上部コントロール
----------------

- Host: gpsd のホスト名または IP アドレス
- Port: gpsd の TCP ポート
- Grid: sky view の仰角グリッド間隔、30 deg または 45 deg
- Projection: Linear または Spherical の仰角マッピング
- Units: 速度と上昇率の表示単位
- Coord: 緯度経度の表示形式
- Time: UTC またはローカル時刻表示
- Raw JSON: デバッグ用 Raw JSON パネルの表示 on/off
- Reset Settings: 保存された設定を消去して初期状態に戻す


衛星一覧
--------

衛星一覧には以下の項目を表示します。

    GNSS, SVID, sigId, PRN, El, Az, SNR, Qual, prRes, Health, Used

数値列は数値としてソートできます。
更新時は衛星キーに基づいて既存行を再利用し、毎回全行を作り直さないように
しています。


Raw JSON デバッグパネル
-----------------------

Raw JSON を有効にすると、gpsd から受信した JSON パケットを画面下部に
表示します。最新 500 行のみ保持します。

通常利用では非表示のままで問題ありません。gpsd の実際の出力内容を確認したい
場合に使用してください。


保存される設定
--------------

このアプリケーションは QSettings を使って以下の設定を保存します。

- Host と Port
- Grid と Projection
- Units、Coord、Time
- sky view の回転角
- PRN 番号表示の on/off
- 凡例表示の on/off
- Raw JSON パネル表示の on/off

保存された設定を初期化したい場合は Reset Settings を使用してください。
