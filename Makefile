# -*- coding: utf-8; tab-width:4; indent-tabs-mode:nil -*-
# Makefile
# 情報システム実験II（ネットワーク）
# 東京都立大学 システムデザイン学部 電子情報システム工学科
# 酒井和哉
# 2019年6月26日

# Makefileについて：
#
# Makefileとは、コンパイルコマンドとファイルとファイル間の依存関係を記す。
# 「make」と入力するだけで、自動的にコマンドを実行しプログラムのコンパイルをしてくれる。
# スクリプトとの違いは、依存関係に基づいて、更新が必要なファイルだけをコンパイルする点である。
#
# 「make」を入力して、Makefileを実行する場合は、ファイル名を「Makefile」にする必要がある。
# 他の名前を使いたい場合は、「make -f file_name」といった要領で、オプションでファイル名を指定する。
# （ファイル名を「make_windows」とした場合は、「make -f make_windows」）

# CC		：コンパイルコマンド
# FILE 		：作成したファイルを全て書く（アーカイブを作成に使う）
# OBJ  		：実行ファイルのリスト
# OBJ_U	    ：ユーティリティ（実行ファイルとリンクするオブジェクトリスト）
# DIR		：アーカイブを作成するときのフォルダ名
CC    	= 	gcc
FILE   	= 	Makefile \
			ft_server.c ft_client.c share_server.c share_client.c \
			error_handler.h error_handler.c
OBJ   	=   ft_server ft_client share_server share_client
OBJ_U	=	error_handler.o
DIR    	=	day3

# all ：
# ターミナルで「make」と入力すると実行するコマンドリスト
# タブでallの範囲を指定する。
# 「-w」は、警告を全て出す、「-o」でコンパイル後の実行ファイル名
# 「$(OBJ_U)」はリンクさせるオブジェクトリスト
# また、「all: $(OBJ_U)」の「$(OBJ_U)」はallを実行するために必要なモノ
all: $(OBJ_U)
	$(CC) -w ft_server.c -o ft_server $(OBJ_U)
	$(CC) -w ft_client.c -o ft_client $(OBJ_U)
	$(CC) -w share_server.c -o share_server $(OBJ_U)
	$(CC) -w share_client.c -o share_client $(OBJ_U)

# オブジェクトを自動でコンパイル
.c.o:
	$(CC) -c $<

error_handler.o: error_handler.h

# アーカイブを作成する時に使う(ターミナルで「make pkg」と入力)
pkg:
	# ディレクトリ作成
	if [ ! -d $(DIR) ]; then \
		mkdir $(DIR); \
	fi
	
	# 「FILE」にリストアップしたファイルをコピー
	for var in $(FILE); do\
		cp $$var $(DIR)/$$var; \
	done \
	
	# tar.gzで圧縮
	tar zcvf $(DIR).tar.gz $(DIR)
	rm -r $(DIR)
	
# ターミナルで「make clean」と入力すると、OBJとOBJ_Uにリストアップしたフィアルを削除
clean:
	rm $(OBJ) $(OBJ_U)
