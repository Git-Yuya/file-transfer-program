#include <stdio.h>
#include <stdint.h>
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

#define BUFF_SIZE 64      // バッファのサイズ
#define FILE_NAME_LEN 16  // ファイル名の長さ

// プロトタイプ宣言
int get_file_size(char *file_name);
int encode_uint8(uint8_t *bytes, uint8_t cmd, int offset);
int encode_str(uint8_t *bytes, char *var, int offset, int size);
int encode_int(uint8_t *bytes, int var, int offset, int size);
int encode_pkt(uint8_t *bytes, uint8_t cmd, char file_name[FILE_NAME_LEN], int file_size);
int decode_uint8(uint8_t *bytes, uint8_t *var, int offset);
int decode_str(uint8_t *bytes, char *file_name, int offset, int size);
int decode_int(uint8_t *bytes, int *file_size, int offset, int size);
int decode_pkt(uint8_t *bytes, uint8_t *cmd, char *file_name, int *file_size);


int main(int argc, char *argv[]) 
{
    // パラメータ
    struct protoent *protocol_entry;          // プロトコルDBからエントリーを取得
    int port_num = 50000;                     // ポート番号
    struct sockaddr_in serv_addr, clnt_addr;  // ソケットアドレス
    int serv_socket, clnt_socket;             // ソケット記述子
    int addr_len;                             // アドレス長
    int n = 0;                                // 戻り値の保存用

    // The input variables to an encoder
    uint8_t reply = 0x1;
    uint8_t data = 0x2;
    uint8_t quit = 0x3;
    uint8_t ack = 0x4;
    char file_name[FILE_NAME_LEN];
    int file_size = 0;

    // The output variables from a decoder.
    uint8_t decoded_cmd;
    char decoded_file_name[FILE_NAME_LEN];
    int decoded_file_size = 0;

    // Encoded data
    uint8_t bytes[21];
    int offset = 0;

    // 並列処理を行うための変数
    pid_t pid;
    // 受信用バッファ
    char buff[BUFF_SIZE];

    // 時間を格納する構造体を宣言
    struct timeval start_time;
    struct timeval end_time;

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

        /* 失敗 */
        if (pid == -1)
        {
            err(EXIT_FAILURE, "can not fork");
        }

        /* 子プロセス */
        else if (pid == 0)
        {
            while (1)
            {
                // パケットを受信
                n = read(clnt_socket, bytes, sizeof(bytes));
                if (n < 0)
                {
                    handle_error("Fail to read a message.\n");
                }

                decoded_file_size = 0;
                decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
                printf("[%d, %s, %d] is received\n", decoded_cmd, decoded_file_name, decoded_file_size);
                
                /*--- クライアントのダウンロード要求 ---*/
                if (decoded_cmd == reply)
                {
                    while (1)
                    {
                        // ファイルの存在判定
                        if (access(decoded_file_name, F_OK) != -1)
                        {
                            printf("%s is found\n", decoded_file_name);
                            break;
                        }
                        else
                        {
                            printf("%s is not found\n", decoded_file_name);

                            // Encoding
                            encode_pkt(bytes, quit, decoded_file_name, file_size);
                            // パケットquitをクライアントに送信
                            n = write(clnt_socket, bytes, sizeof(bytes));
                        }

                        // パケットを受信
                        n = read(clnt_socket, bytes, sizeof(bytes));
                        if (n < 0)
                        {
                            handle_error("Fail to read a message.\n");
                        }

                        decoded_file_size = 0;
                        decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
                        printf("[%d, %s, %d] is received\n", decoded_cmd, decoded_file_name, decoded_file_size);
                    }

                    // ファイルサイズを取得
                    file_size = get_file_size(decoded_file_name);

                    // 符号化
                    encode_pkt(bytes, ack, decoded_file_name, file_size);
                    // クライアントにACKを送信
                    n = write(clnt_socket, bytes, sizeof(bytes));
                    if (n < 0)
                    {
                        handle_error("Fail to write a message.\n");
                    }

                    // ファイルデータを送信（64バイトずつ）
                    // ファイル１（コピー元）を読み込みモードで開く。
                    int fd_in = open(decoded_file_name, O_RDONLY);
                    if (fd_in < 0) 
                    {
                        handle_error("Fail to open a file.");
                    }
                    // 64バイトずつ送信
                    do 
                    {
                        // 引数：１）ファイル記述子、２）読み込んだバイトを格納する変数、３）読み込むバイト数
                        // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                        n = read(fd_in, buff, sizeof(buff));
                        if (n < 0)
                        {
                            handle_error("Fail to read bytes from the input file.\n");
                        }
                        
                        // 書き出し。
                        // 引数：１）ファイル記述子、２）書き出すバイトが格納された変数、３）書き込むバイト数
                        // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                        n = write(clnt_socket, buff, n);
                        if (n < 0) 
                        {
                            handle_error("Fail to read bytes from the input file.\n");
                        }
                        
                        // コピー元ファイルを全て読みこめば、nの値が0となる。nが1以上でなければ、ループを抜ける。
                    } 
                    while (n > 0);

                    // ファイルを閉じる。
                    close(fd_in);

                    // クライアントからACKを受信
                    n = read(clnt_socket, bytes, sizeof(bytes));
                    if (n < 0)
                    {
                        handle_error("Fail to read a message.\n");
                    }

                    decoded_file_size = 0;
                    decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
                    if (decoded_cmd == ack)
                    {
                        printf("ACKを受信(ファイルダウンロード完了)\n");
                    }
                }
                
                /*--- クライアントのアップロード要求 ---*/
                else if (decoded_cmd == data)
                {
                    // クライアントにACKを送信
                    encode_pkt(bytes, ack, decoded_file_name, decoded_file_size);
                    n = write(clnt_socket, bytes, sizeof(bytes));
                    if (n < 0)
                    {
                        handle_error("Fail to write a message.\n");
                    }

                    // ファイルデータを受信（64バイトずつ）
                    // コピー先のファイル名
                    char copy_file_name[FILE_NAME_LEN + 10] = "upload_";
                    strcat(copy_file_name, decoded_file_name);

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
                        n = read(clnt_socket, buff, sizeof(buff));
                        if (n < 0) 
                        {
                            handle_error("Fail to read bytes from the input file.\n");
                        }
                        
                        // 書き出し
                        // 引数：１）ファイル記述子、２）書き出すバイトが格納された変数、３）書き込むバイト数
                        // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                        n = write(fd_out, buff, n);
                        if (n < 0) 
                        {
                            handle_error("Fail to read bytes from the input file.\n");
                        }

                        // nの値が64でなければ、ループを抜ける。
                    }
                    while (n == BUFF_SIZE);

                    // ファイルを閉じる。
                    close(fd_out);

                    // クライアントにACKを送信
                    encode_pkt(bytes, ack, decoded_file_name, decoded_file_size);
                    n = write(clnt_socket, bytes, sizeof(bytes));
                    if (n < 0)
                    {
                        handle_error("Fail to write a message.\n");
                    }
                }

                /*--- クライアントからの終了コマンド ---*/
                else if (decoded_cmd == quit)
                {
                    printf("コネクションが閉じました。\n");
                    putchar('\n');
                    break;
                }

                putchar('\n');
            }

            // クライアントのソケットを閉じる。
            close(clnt_socket);
        }

        /* 親プロセス */
        else
        {
            putchar('\n');
        }
    }

    // 受付用のソケットを閉じる。
    close(serv_socket);
    return 0;
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
    if (fd < 0)
    {
        handle_error("Fail to open a file.");
    }
  
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

// This function encodes an 8-bit unsigned integer into a byte.
int encode_uint8(uint8_t *bytes, uint8_t cmd, int offset) 
{
    bytes[offset++] = cmd;
    return offset;
}

// This function encodes a string into a bytecode by big-endian.
int encode_str(uint8_t *bytes, char *var, int offset, int size) 
{
    int i;

    for (i = 0; i < size; i++) 
    {
        bytes[offset++] = (uint8_t)var[i];
    }

    return offset;
}

// This function encodes a 4-byte integer into a bytecode by big-endian.
int encode_int(uint8_t *bytes, int var, int offset, int size) 
{
    int i;
    for (i = 0; i < size; i++) 
    {
        bytes[offset++] = (uint8_t)(var >> ((size - 1) - i) * 8);
    }

    return offset;
}

// This function encodes a packet into a bytecode.
int encode_pkt(uint8_t *bytes, uint8_t cmd, char file_name[FILE_NAME_LEN], int file_size) 
{
    int offset;

    offset = encode_uint8(bytes, cmd, 0);
    offset = encode_str(bytes, file_name, offset, FILE_NAME_LEN);
    offset = encode_int(bytes, file_size, offset, sizeof(file_size));

    return offset;
}

/*
 * Decoding
 */

// This function decodes a byte to an 8-bit integer.
int decode_uint8(uint8_t *bytes, uint8_t *var, int offset) 
{
    *var = bytes[offset++];

    return offset;
}

// This function decodes a bytecode to a string.
int decode_str(uint8_t *bytes, char *file_name, int offset, int size) 
{
    int i;
    for (int i = 0; i < size; i++)
    {
        file_name[i] = bytes[offset++];
    }

    return offset;
}

// This function decodes a bytecode to a 4-byte integer.
int decode_int(uint8_t *bytes, int *file_size, int offset, int size) 
{
    int i;
    for (int i = 0; i < size; i++)
    {
        *file_size += bytes[offset++] << ((size - 1) - i) * 8;
    }

    return offset;
}

// This functin decodes a bytecode to a packet.
int decode_pkt(uint8_t *bytes, uint8_t *cmd, char *file_name, int *file_size) 
{
    int offset = 0;

    offset = decode_uint8(bytes, cmd, offset);
    offset = decode_str(bytes, file_name, offset, FILE_NAME_LEN);
    offset = decode_int(bytes, file_size, offset, sizeof(int));

    return offset;
}
