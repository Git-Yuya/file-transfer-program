#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include "error_handler.h"

#define BUFF_SIZE 64   // バッファのサイズ

int main(int argc, char *argv[]) 
{
    // パラメータ
    struct protoent *protocol_entry;          // プロトコルDBからエントリーを取得
    int port_num = 50000;                     // ポート番号
    struct sockaddr_in serv_addr, clnt_addr;  // ソケットアドレス
    int serv_socket, clnt_socket;             // ソケット記述子
    int addr_len;                             // アドレス長
    int n = 0;                                // 戻り値の保存用

    // 送信用
    char ACK[BUFF_SIZE] = "ACK";

    // 受信用
    char file_name[64];
    int file_size = -1;
    char file_size_c[64];
    char data[64];
    char buff[BUFF_SIZE];

    // 並列処理のための変数
    pid_t pid;

    // パラメータの初期化
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_num);
    addr_len = sizeof (clnt_addr);

    // プロトコルエントリーを取得
    // "tcp"を引数にすると、TCPに関するエントリーを取得
    protocol_entry = getprotobyname("tcp");
    if (protocol_entry == NULL) 
    {
        handle_error("Unknown protocol\n");
    }

    // 接続要求受付用のソケットを作成
    // ソケット記述子（Socket descripter）が戻り値であるが、エラーが起こった場合は「-1」が返される。
    serv_socket = socket(AF_INET, SOCK_STREAM, protocol_entry->p_proto);
    if (serv_socket < 0) 
    {
        handle_error("Fail to create socket.\n");
    }

    // バインド（ソケットとポートの結合）
    if (bind(serv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        handle_error("Fail to bind a socket.");
    }

    // ソケットをコネクション受け入れ可能な状態にする。
    // 第２引数は、接続キューのサイズ。５つまで同時接続を受け入れると指定。
    if (listen(serv_socket, 5) < 0) 
    {
        handle_error("Fail to listen to a socket.\n");
    }

    // クライアントから接続要求があれば、順次対応
    while (1) 
    {
        // accept(.)により、クライアントからの接続要求を受け付ける。
        // 戻り値はクライアントとのデータ通信用ソケット記述子、エラーの場合は0以下の値が返される。
        fprintf(stderr, "Waiting for a client...\n");
        clnt_socket = accept(serv_socket, (struct sockaddr *) &clnt_addr, &addr_len);

        // クライアントのIPアドレスとポート番号を表示
        // それぞれ、struct sockaddr_inから取得
        // inet_ntoa(.)は、arpa/inet.hで定義されている（Unix系の場合）。
        fprintf(stderr, "クライアントからコネクションを受け付けました。\n");
        fprintf(stderr, "IPアドレス: %s\n", inet_ntoa(clnt_addr.sin_addr));
        fprintf(stderr, "ポート番号: %d\n", clnt_addr.sin_port);

        pid = fork();

        // 失敗
        if (pid == -1)
        {
            err(EXIT_FAILURE, "can not fork");
        }

        // 子プロセス
        else if (pid == 0)
        {
            while (1)
            {
                /*--- ファイル名を受信 ---*/
                n = read(clnt_socket, file_name, sizeof(file_name));
                if (n < 0) 
                {
                    handle_error("Fail to read a message from socket.\n");
                }
                printf("受信したファイル名: %s\n", file_name);
                // クライアントにACKを返す。
                n = write(clnt_socket, ACK, sizeof(buff));
                if (n < 0) 
                {
                    handle_error("Fail to write a message.\n");
                }

                /*--- ファイルサイズを受信 ---*/
                n = read(clnt_socket, file_size_c, sizeof(file_size_c));
                if (n < 0) 
                {
                    handle_error("Fail to read a message from socket.\n");
                }
                file_size = atoi(file_size_c);
                printf("受信したファイルサイズ: %d [bytes]\n", file_size);
                // クライアントにACKを返す。
                n = write(clnt_socket, ACK, sizeof(buff));
                if (n < 0) 
                {
                    handle_error("Fail to write a message.\n");
                }

                /*--- ファイルデータを受信（64バイトずつ）---*/
                // コピー先のファイル名
                char copy_file_name[64 + 6] = "copy_";
                strcat(copy_file_name, file_name);

                // ファイル２（コピー先）を開く。
                // O_CREAT 新規作成、OWRONLY 読み書き、S_IRWXU パミッション
                int fd_out = open(copy_file_name, O_CREAT | O_WRONLY, S_IRWXU);
                if (fd_out < 0) 
                {
                    handle_error("Fail to open a file.");
                }

                // 64バイトずつバッファに読み込んで、書き出す。
                do 
                {
                    // 引数：１）ファイル記述子、２）読み込んだバイトを格納する変数、３）読み込むバイト数
                    // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                    n = read(clnt_socket, data, sizeof(data));
                    if (n < 0) 
                    {
                        handle_error("Fail to read bytes from the input file.\n");
                    }
                    
                    // 書き出し
                    // 引数：１）ファイル記述子、２）書き出すバイトが格納された変数、３）書き込むバイト数
                    // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                    n = write(fd_out, data, n);
                    if (n < 0) 
                    {
                        handle_error("Fail to read bytes from the input file.\n");
                    }

                    // nの値が64でなければ、ループを抜ける。
                }
                while (n == BUFF_SIZE);

                // ファイルを閉じる。
                close(fd_out);

                // クライアントにACKを返す。
                n = write(clnt_socket, ACK, sizeof(ACK));
                if (n < 0) 
                {
                    handle_error("Fail to write a message.\n");
                }
                
                /*--- 終了コマンド判定 ---*/
                n = read(clnt_socket, buff, sizeof(buff));
                if (n < 0) 
                {
                    handle_error("Fail to read a message from socket.\n");
                }
                if (strcmp(buff, "q") == 0)
                {
                    printf("コネクションが閉じました。\n");
                    putchar('\n');
                    break; 
                }
            }

            // クライアントのソケットを閉じる。
            close(clnt_socket);
        }

        // 親プロセス
        else
        {
            putchar('\n');
        }
    }

    // 受付用のソケットを閉じる。
    close(serv_socket);
    return 0;
}
