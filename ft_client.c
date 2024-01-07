#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include "error_handler.h"

#define BUFF_SIZE 64   // バッファのサイズ

// プロトタイプ宣言
int get_file_size(char *file_name);


int main(int argc, char *argv[]) 
{
    // 時間を格納する構造体を宣言
    struct timeval start_time;
    struct timeval end_time;

    // サーバのアドレスとポート番号
    // 127.0.0.1は、ループバックアドレス
    // 他のPCと通信する場合は、当該PCのIPアドレスに変更する。
    // char *serv_ip = "172.18.85.234";
    char *serv_ip = "127.0.0.1";
    in_port_t serv_port = 50000;

    // 受信用バッファ：戻り値の保存用に使う変数
    char buff[BUFF_SIZE];
    int n = 0;

    // 送信用
    char file_name[64];
    int file_size = -1;
    char file_size_c[64];
    char data[64];

    // ソケット作成、入力はIP、ストリーム型、TCPを指定
    int socketd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketd < 0) 
    {
        handle_error("Fail to create a socket.\n");
    }

    // サーバのアドレス等を初期化
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
    serv_addr.sin_port = htons(serv_port);

    // サーバに接続
    n = connect(socketd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (n < 0) 
    {
        handle_error("Fail to connect to the server.\n");
    }

    // サーバとの通信
    while (1)
    {
        while (1)
        {
            // ユーザからのファイル名入力を待機
            printf("サーバに送信するファイル名: ");
            scanf("%s", file_name);

            // ファイルの存在判定
            if (access(file_name, F_OK) != -1)
            {
                printf("%s is found\n", file_name);
                break;
            }
            else
            {
                printf("%s is not found\n", file_name);
            }
        }

        // ファイルサイズを取得
        file_size = get_file_size(file_name);
        // ファイルサイズを文字列型に変換
        snprintf(file_size_c, 64, "%d", file_size);

        gettimeofday(&start_time, NULL);

        /*--- ファイル名を送信 ---*/
        n = write(socketd, file_name, sizeof(file_name));
        if (n < 0) 
        {
            handle_error("Fail to write a message.\n");
        }
        // サーバからACKを受信
        n = read(socketd, buff, sizeof(buff));
        if (n < 0)
        {
            handle_error("Fail to read a message.\n");
        }
        if (strcmp(buff, "ACK") == 0)
        {
            printf("ACKを受信(ファイル名受信)\n");
        }

        /*--- ファイルサイズを送信 ---*/
        n = write(socketd, file_size_c, sizeof(file_size_c));
        if (n < 0) 
        {
            handle_error("Fail to write a message.\n");
        }
        // サーバからACKを受信
        n = read(socketd, buff, sizeof(buff));
        if (n < 0)
        {
            handle_error("Fail to read a message.\n");
        }
        if (strcmp(buff, "ACK") == 0)
        {
            printf("ACKを受信(ファイルサイズ受信)\n");
        }

        /*--- ファイルデータを送信（64バイトずつ） ---*/
        // ファイル１（コピー元）を読み込みモードで開く。
        int fd_in = open(file_name, O_RDONLY);
        if (fd_in < 0) 
        {
            handle_error("Fail to open a file.");
        }

        // 64バイトずつ送信
        do 
        {
            // 引数：１）ファイル記述子、２）読み込んだバイトを格納する変数、３）読み込むバイト数
            // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
            n = read(fd_in, data, sizeof(data));
            if (n < 0)
            {
                handle_error("Fail to read bytes from the input file.\n");
            }
            
            // 書き出し
            // 引数：１）ファイル記述子、２）書き出すバイトが格納された変数、３）書き込むバイト数
            // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
            n = write(socketd, data, n);
            if (n < 0) 
            {
                handle_error("Fail to read bytes from the input file.\n");
            }
            
            // コピー元ファイルを全て読みこめば、nの値が0となる。nが1以上でなければ、ループを抜ける。
        } 
        while (n > 0);

        // ファイルを閉じる。
        close(fd_in);

        // サーバからACKを受信
        n = read(socketd, buff, sizeof(buff));
        if (n < 0)
        {
            handle_error("Fail to read a message from socket.\n");
        }
        if (strcmp(buff, "ACK") == 0)
        {
            printf("ACKを受信(ファイル受信完了)\n");
        }

        gettimeofday(&end_time, NULL);

        // RTTを計算
        double diff_sec = end_time.tv_sec - start_time.tv_sec;
        double diff_usec = end_time.tv_usec - start_time.tv_usec;
        double diff_time = diff_sec + diff_usec * 0.001 * 0.001; 
        printf("RTT = %f [s]\n", diff_time);

        // 終了コマンドが入力されたらプログラム終了
        printf("qが入力されたら終了し、それ以外なら続行: ");
        scanf("%s", buff);
        n = write(socketd, buff, sizeof(buff));
        if (n < 0) 
        {
            handle_error("Fail to write a message.\n");
        }
        if (strcmp(buff, "q") == 0)
        {
            printf("終了します。\n");
            break;
        }

        putchar('\n');
    }

    // ソケットを閉じる。
    close(socketd);
}

/*
 * 引数で受け取ったファイル名のファイルサイズを返す。
 */
int get_file_size(char *file_name) 
{
    int fd;         // ファイル記述子（A file descriptor）
    int file_size;  // ファイルサイズ

    // ファイルを開く。
    fd = open(file_name, 0);
    if (fd < 0) handle_error("Fail to open a file.");

    // ポインタをファイルの最後に移動させる。lseek(.)は現在のポインタを返す。
    // 従って、ファイルポインタをファイルの最後に移動させれば、ファイルのバイト数が分かる。
    // 引数：ファイル記述子、オフセット、オフセットからのバイト数
    // SEEK_ENDはファイルの最後を指す。
    file_size = lseek(fd, 0, SEEK_END);
    // ポインタをファイルの先頭に戻す。
    lseek(fd, 0, SEEK_SET);

    // ファイルを閉じる。
    close(fd);

    return file_size;
}
