[English](README.md) | [日本語](README.ja.md)

[![windows](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/windows.yml/badge.svg)](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/windows.yml)
[![macos](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/macos.yml/badge.svg)](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/macos.yml)
[![ubuntu](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/renatus-novus-x/ble-motion-demo/actions/workflows/ubuntu.yml)

<p align="center">
  <img src="images/teaser.gif" alt="X68000で動作するble-motion-demo">
</p>

ble-motion-demo
===============

<p align="center">
  <strong><a href="https://uraraworks.github.io/WebX68k/?cpu=10&amp;ram=12&amp;fd1=https://raw.githubusercontent.com/renatus-novus-x/ble-motion-demo/main/dist/ble-motion-demo.zip&amp;run=1">WebX68kでble-motion-demoを起動</a></strong>
</p>

X68000をシリアル通信とBLE（Bluetooth Low Energy）でiPhoneにつなぐ実験のために作ったデモプログラムです。
アプリ「Bluefruit Connect」から送られる傾きや向きのデータを受信し、画面上の白いワイヤーフレームの立方体を回転させます。Bluefruit ConnectにはiOS版とAndroid版があります。アプリの導入方法は[公式セットアップガイド](https://learn.adafruit.com/bluefruit-le-connect/ios-setup)を参照してください。

X68000側はシリアルポートを使い、Windows上の中継ソフトがシリアル通信とBLEを橋渡しします。
WebX68kでも試せるほか、Windows・macOS・Ubuntuで動作するデスクトップ版も用意しています。

<details>
<summary>対応環境</summary>

| 環境 | 描画ライブラリ |
| --- | --- |
| Windows | freeglut 3.4.0 / OpenGL |
| macOS | Apple GLUT / OpenGL frameworks |
| Ubuntu | freeglut 3.4.0 / OpenGL |
| X68000 / Human68k | IOCS 16色グラフィックページ上のOpenGL/GLUT互換API |

</details>

<details>
<summary>実行と操作</summary>

### Windows側の接続を準備する

初めて試す場合は、[Windows COM9–BLE接続セットアップ手順（PDF）](docs/windows-com9-ble-setup.pdf)を開き、次の順に準備してください。Windows上のWebX68kとWindowsデスクトップ版で共通の準備です。

com0comは、互いにつながった2つの仮想シリアルポートをWindows上に作るドライバーです。片側に書き込んだデータをもう片側で受け取れます。この構成では片側を`COM9`、もう片側を`BLE`と名付けます。`BLE`は仮想ポートの名前であり、無線通信自体は中継ソフトのble-serialが担当します。

```text
Bluefruit Connect → BLE無線通信 → ble-serial → BLE側ポート
                 → com0com → COM9 → WebX68kまたはWindows版デモ
```

1. **必要なものを用意する（PDF p.2）。** Windows PC、Windows上でBLE Peripheralとして動作できるBluetoothアダプター、Bluefruit Connectを入れたiPhoneまたはAndroidスマートフォンを用意します。PCが接続を待ち、スマートフォンからPCを探して接続する構成です。
2. **com0comを導入する。** PDFはcom0comのインストール済み環境を前提にしています。未導入の場合は、先に[com0com公式サイト](https://com0com.sourceforge.net/)の配布物と導入案内を確認してインストールしてください。
3. **仮想ポートのペアを用意する（PDF p.3）。** 管理者PowerShellで既存ポートを確認してから、`COM9`と`BLE`のペアを作成します。同じペアがある場合は再利用します。他の機器が使っているCOM9を変更したり、同名のポートを重複作成したりしないでください。
4. **Pythonと中継ソフトを設定する（PDF p.4–6）。** 通常のPowerShellで専用のPython 3.11仮想環境を作成し、PDFに記載されたライブラリのバージョンと回避修正を適用します。
5. **BLE中継を起動する（PDF p.7）。** 確認コマンドの結果を確認し、PDFのServer起動コマンドを実行します。ble-serialが開くのは`BLE`側です。起動成功のログが出たら、そのPowerShellを開いたままにしてください。PowerShellを開き直した場合は、PDF p.4の`$blePython`の代入を再実行します。変数に入れたPythonを実行するときは先頭の`&`が必要です。

PDF p.8のUART端末と`CTTY AUX`は、文字を送受信する別の使用例です。**このデモでは`CTTY AUX`を実行せず、以下のController操作を使います。**

### WebX68kで試す

1. 上記のWindows側の準備を完了し、BLE中継を起動しておきます。
2. 冒頭のWebX68k起動リンクをWindows PCのWeb Serial対応ブラウザーで開きます。ディスクからデモが自動起動し、静止した立方体が表示されます。
3. WebX68kのシリアル接続でボーレートを`38400 bps`に設定し、`COM9`を選んで接続します。ここで選ぶのは`BLE`側ではありません。
4. 次のBluefruit Connectの操作で、スマートフォンから姿勢データを送ります。

COM9をWebX68kとWindowsデスクトップ版で同時に開かないでください。切り替えるときは、先に使用中のアプリのシリアル接続を終了します。

### Bluefruit Connectから姿勢データを送る

以下はiOS版の画面例です。端末名を置き換え、周辺機器やステータスバーの情報を除いています。Android版では画面の配置や表示が異なる場合があります。

![Bluefruit Connectの操作例：接続先を選択、Controllerを開く、Quaternionを有効化](images/bluefruit-connect-setup.png)

1. **接続先を選ぶ。** Select Device画面で、自分のWindows PCのBLE中継を探し、`Connect`を押します。画像の`BLE Bridge`は説明用の名前です。実際には`WebX68k`やWindows PC名などが表示される場合があるため、ご自身の接続先を選んでください。
2. **Controllerを開く。** 接続後のModules画面で`Controller`を選びます。`UART`ではありません。
3. **Quaternionを有効にする。** `Quaternion`のスイッチをオンにします。Quaternionはスマートフォンの向きを表すデータです。このデモでは他のセンサーのスイッチをオンにする必要はありません。

スマートフォンを傾けたり回したりすると、デモの立方体が回転します。動かない場合は、BLE中継が動作中であること、接続先が自分のPCであること、COM9の接続と`38400 bps`の設定、Quaternionのスイッチを順に確認してください。

### ローカルでビルドした実行ファイルを使う

以下のデスクトップ版のコマンドは、ビルドとインストール手順を完了した状態で実行します。
Windowsで姿勢入力を使わずに実行:

```powershell
.\out\bin\ble-motion-demo.exe
```

Windowsでcom0comのCOM9とBLE側の仮想ポートを接続する準備は、[Windows COM9–BLE接続セットアップ手順（PDF）](docs/windows-com9-ble-setup.pdf)を参照してください。

Windowsでcom0comのCOM9を38400 bpsで使用:

```powershell
.\out\bin\ble-motion-demo.exe COM9 38400
```

macOS・Ubuntuで姿勢入力を使わずに実行:

```sh
./out/bin/ble-motion-demo
```

macOS・Ubuntuで姿勢入力を受信する場合は、シリアルデバイスとボーレートを指定します。`/dev/tty.example`は実際のデバイスパスに置き換えてください。

```sh
./out/bin/ble-motion-demo /dev/tty.example 38400
```

X68000版は引数なしで起動します。本体RS-232Cを38400 bps固定（8N1、フロー制御なし）で自動初期化し、受信を開始します。

```text
bledemo.x
```

配布XDFではHuman68k起動後にこのコマンドを自動実行します。

- デスクトップ版は512×512のウィンドウで起動します。X68000版は512×512固定の描画領域を使います。
- デスクトップ版はウィンドウをリサイズしても立方体の縦横比を維持します。
- EscまたはQで終了します。

</details>

<details>
<summary>デスクトップ版のビルド</summary>

CMake 3.21以上とCコンパイラーを用意してください。
WindowsはVisual Studioの「C++によるデスクトップ開発」、macOSはXcode Command Line Toolsを使用できます。
Windows・UbuntuではGitも必要です。CMakeがfreeglutを取得するため、初回の構成時にネットワーク接続が必要です。

Ubuntuでは依存パッケージをインストールします。

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake git libgl1-mesa-dev libglu1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxxf86vm-dev
```

clone済みのリポジトリ内で、デスクトップ3環境共通のビルドコマンドを実行します。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix out
```

</details>

<details>
<summary>X68000版のビルドと配布物作成</summary>

### WSLにUbuntu 24.04を導入する

WindowsでPowerShellを管理者として開き、Ubuntu 24.04をインストールします。

```powershell
wsl --install -d Ubuntu-24.04
```

再起動を求められた場合はWindowsを再起動し、PowerShellからUbuntuを開きます。

```powershell
wsl -d Ubuntu-24.04
```

初回起動時は案内に従い、Linux用のユーザー名とパスワードを作成します。
Ubuntu 24.04を導入済みの場合はインストールを省略し、上記の起動コマンドを実行してください。
導入の詳細は[WSL公式インストール手順](https://learn.microsoft.com/ja-jp/windows/wsl/install)を参照してください。

### Ubuntu内にelf2x68kを導入する

以降はUbuntuのシェルで実行します。ここでは
[elf2x68kの20260602リリース](https://github.com/yunkya2/elf2x68k/releases/tag/20260602)のLinux版を使用します。
XDFとZIPの作成に必要なパッケージもまとめて導入します。

```sh
sudo apt-get update
sudo apt-get install -y bzip2 make curl ca-certificates python3 unar
cd "$HOME"
curl --fail --location --remote-name https://github.com/yunkya2/elf2x68k/releases/download/20260602/elf2x68k-Linux-20260602.tar.bz2
tar -xjf elf2x68k-Linux-20260602.tar.bz2 && rm elf2x68k-Linux-20260602.tar.bz2
```

コンパイラーのディレクトリをログイン時のPATHへ追加します。追記は一度だけ行い、設定を読み込みます。

```sh
echo 'export PATH="$HOME/m68k-xelf/bin:$PATH"' >> "$HOME/.profile"
source "$HOME/.profile"
m68k-xelf-gcc --version
```

シングルクォートで囲むことで、`.profile`への追記時には`$HOME`と`$PATH`を展開せず、
設定を読み込むときに展開します。

### 実行ファイルとディスクイメージをビルドする

Ubuntu内でclone済みのリポジトリへ移動し、makeを実行します。
Windowsのドライブは`/mnt/c`や`/mnt/d`から参照できます。
下記のパスは、ご自身のリポジトリの配置先に置き換えてください。

```sh
cd /mnt/c/path/to/ble-motion-demo
make -f Makefile.x68k
```

次のファイルが生成されます。

- `dist/bledemo.x`: Human68k実行ファイル
- `dist/ble-motion-demo.xdf`: `bledemo.x`を自動実行する起動可能なHuman68kディスク
- `dist/ble-motion-demo.zip`: XDFとHuman68k許諾文を収録したWebX68k向け配布ファイル

</details>

<details>
<summary>自動ビルドとダウンロード</summary>

GitHub Actionsで、push・pull request・手動実行時にWindows・macOS・Ubuntu版を個別にビルドします。X68000版は上記の手順でローカルビルドします。
先頭のバッジは、デスクトップ3環境のビルド結果を表示します。
各実行のArtifactsから実行ファイルを取得できます。
ビルド成功はGUIの実動作確認を意味しません。

</details>

<details>
<summary>目的</summary>

- デスクトップ環境とX68000で描画・姿勢パケット解析コードを共有する
- C90相当の簡潔な構文で、古い環境へ移植しやすくする
- デスクトップ版のGLUT/OpenGLとX68000版のIOCS描画を分離する

</details>

<details>
<summary>ファイル構成</summary>

- `src/main.c`: シリアル受信、Bluefruitパケット解析、立方体の描画
- `src/platform.h`: 機種依存のシリアルポート、時刻取得、待機処理
- `src/x68k/GL/`: X68000専用OpenGL/GLUT互換ヘッダー
- `src/x68k/glut.c`: X68000向けOpenGL/GLUT互換IOCS描画バックエンド
- `CMakeLists.txt`: ビルドと描画ライブラリの設定
- `Makefile.x68k`: X68000クロスビルド設定
- `scripts/patch_autoexec.py`: Human68k自動起動設定
- `scripts/make_sjis_zip.py`: 配布ZIP作成
- `.github/workflows/windows.yml`: Windowsビルド
- `.github/workflows/macos.yml`: macOSビルド
- `.github/workflows/ubuntu.yml`: Ubuntuビルド

</details>

<details>
<summary>注意点 / 制限</summary>

- Bluefruit ConnectのControllerにあるセンサー一覧からQuaternionを選びます。正常な`!Q`パケットを受信すると立方体が回転します。
- `idle()`でシリアル入力を非ブロッキングで確認します。デスクトップ版は次の1/60秒のフレーム予定時刻までの残り時間だけ待ちます。X68000版は`glutSwapBuffers()`で次の垂直表示周期を待ってからページを切り替えます。
- 既定のボーレートは38400 bpsです。送受信側で同じ値に設定します。
- アプリケーションはC90相当の構文を使い、8頂点と12辺を`GL_LINES`で描画します。
- macOSのApple OpenGL/GLUTは非推奨APIのため、ビルド時に警告が出ることがあります。
- Ubuntuの実行にはOpenGLとX11を利用できるデスクトップ環境が必要です。
- X68000版はIOCS経由で本体RS-232Cを初期化・受信します。通信速度は38400 bps固定で、ポートや速度の引数は受け付けません。WebX68k側のシリアル接続も38400 bpsに設定してください。
- X68000版はCRTCを約60 Hzに設定し、表示ページ切替をVDISPに同期します。
- `main.c`はX68000を含む全環境で、`glClear()`、`glBegin(GL_LINES)`、`glFlush()`、`glutSwapBuffers()`など同じOpenGL/GLUT呼び出しを使用します。
- X68000版は専用のOpenGL/GLUT互換バックエンドと、16色モードの512×512 IOCSグラフィックページを2面使います。X68000版の`glClear()`は非表示ページに記録された旧辺だけを黒線で消し、`glutSwapBuffers()`はVDISPで表示ページと描画ページを交換します。毎フレームの全画面消去やソフトウェアフレームバッファの全面転送は行いません。

</details>
