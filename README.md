# ファイル転送プログラム
TCPを利用したファイル転送プログラム(ftプログラムとshareプログラム)を実装した。

## ftプログラム
クライアントが任意のファイルをサーバへ送信するファイル転送プログラム
- サーバの機能 (ft_server.c)
  - クライアントを待ち受け、接続があれば、子プロセスを作成する。
  - ファイル名、ファイルサイズ、ファイルデータを順に受信し、それぞれACKを返す。
  - クライアントから終了コマンドを受信すると、子プロセスを終了する。

- クライアントの機能 (ft_client.c)
  - scanf()でファイル名を受け取り、ファイル名、ファイルサイズ、ファイルデータを順にサーバへアップロードする。この時、サーバからそれぞれのACKを受信する。
  - 複数のファイルをアップロードできるように、ループ文でユーザからのコマンドを受け付ける。この時、TCPコネクションは切断しない。
  - 終了コマンドが入力されると、サーバに終了コマンドを送信し、クライアントプログラムを終了する。
  - バッファサイズの上限は64バイトであり、一度にファイル全体を読み込んでデータを送信できない。そのため、ファイルデータを64バイトずつ読み込み、送信する。

<img width="960" alt="ft" src="https://github.com/Git-Yuya/file-transfer-program/assets/84259422/1f237f8e-3db3-4eea-aed9-9cc681b579a4">

## shareプログラム
符号化・復号化を組み込み、柔軟で保守性の高いファイル転送プログラム
- サーバの機能 (share_server.c)
  - ftプログラムを拡張し、クライアントからのリクエストに応じて、ファイルの送受信を決定する。

- クライアントの機能 (ft_client.c)
  - ターミナルからのコマンドを受け付け、ファイルの送受信を決定する。

<img width="960" alt="share" src="https://github.com/Git-Yuya/file-transfer-program/assets/84259422/fbf0c8b2-0829-4a5b-a732-635c37891077">

## 実装
- 言語：
  <img src="https://img.shields.io/badge/-C%E8%A8%80%E8%AA%9E-A8B9CC.svg?logo=c&style=plastic">
- 統合開発環境：
  <img src="https://img.shields.io/badge/-Visual%20Studio%20Code-007ACC.svg?logo=visualstudiocode&style=plastic">

## コンパイル手順
- makeとgccをインストール
- Makefileがあるディレクトリでmakeコマンドを実行

## 実行手順
- サーバプログラムを実行後、別のターミナルでクライアントプログラムを実行する。以下にUbuntuでの実行例を示す。 <br>
ターミナル1：./share_server <br>
ターミナル2：./share_client <br>
ターミナル3：./share_client
