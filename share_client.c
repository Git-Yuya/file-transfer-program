#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
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
static void handler(int sig);


int main(int argc, char *argv[]) 
{
    // サーバのアドレスとポート番号
    // 127.0.0.1は、ループバックアドレス
    // 他のPCと通信する場合は、当該PCのIPアドレスに変更する。
    // char *serv_ip = "172.28.225.13";
    char *serv_ip = "127.0.0.1";
    in_port_t serv_port = 50000;

    // The input variables to an encoder
    uint8_t request = 0x1;
    uint8_t data = 0x2;
    uint8_t quit = 0x3;
    uint8_t ack = 0x4;
    char file_name[FILE_NAME_LEN];
    int file_size = 0;

    // The output variables from a decoder
    uint8_t decoded_cmd;
    char decoded_file_name[FILE_NAME_LEN];
    int decoded_file_size = 0;

    // Encoded data
    uint8_t bytes[21];
    int offset = 0;

    // 戻り値の保存用に使う変数
    int n = 0;
    // コマンドの種類を判定する変数
    int cmd = -1;
    // 受信用バッファ
    char buff[BUFF_SIZE];

    // 時間を格納する構造体を宣言
    struct timeval start_time;
    struct timeval end_time;

    // シグナル変数の初期化
    struct sigaction act = 
    {
        .sa_handler = handler,  // コールバック関数の指定
        .sa_flags = 0,          // シグナル処理のオプション指定
    };

    sigemptyset(&act.sa_mask);
    if (sigaction(SIGALRM, &act, NULL) < 0)
    {
        return 1;
    }

    // ソケット作成、入力はIP、ストリーム型、TCPを指定。
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
        printf("1:ダウンロード 2:アップロード 3:終了\n");
        printf("数字: ");  scanf("%d", &cmd);

        /*--- ダウンロード ---*/
        if (cmd == 1)
        {
            while (1)
            {
                // ユーザからのファイル名入力を待機
                printf("サーバからダウンロードするファイル名: ");  scanf("%s", file_name);

                file_size = 0;
                
                // The packet to be encoded
                printf("request = [%x, %s, %d]\n", request, file_name, file_size);

                // Encoding
                encode_pkt(bytes, request, file_name, file_size);

                // Print the bytes code.
                printf("E(request) =");
                for (int i = 0; i < 21; i++)
                {
                    printf(" %x", bytes[i]);
                }
                putchar('\n');

                gettimeofday(&start_time, NULL);

                // パケットrequestをサーバに送信
                n = write(socketd, bytes, sizeof(bytes));
                if (n < 0)
                {
                    handle_error("Fail to write a message.\n");
                }

                // サーバからACKかquitを受信
                n = read(socketd, bytes, sizeof(bytes));
                if (n < 0)
                {
                    handle_error("Fail to read a message.\n");
                }

                decoded_file_size = 0;
                decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
                if (decoded_cmd == ack)
                {
                    printf("ACKを受信(パケットrequestの受信)\n");
                    printf("%sをダウンロード中\n", decoded_file_name);
                    printf("%sのファイルサイズ: %d\n", decoded_file_name, decoded_file_size);

                    break;
                }
                else if (decoded_cmd == quit)
                {
                    printf("サーバ側に%sは存在しません。\n", decoded_file_name);
                }
            }

            // ファイルデータを受信（64バイトずつ）
            // コピー先のファイル名
            char copy_file_name[FILE_NAME_LEN + 10] = "download_";
            strcat(copy_file_name, file_name);

            // ファイル２（コピー先）を開く。
            // O_CREAT 新規作成、OWRONLY 読み書き、S_IRWXU パミッション
            int fd_out = open(copy_file_name, O_CREAT | O_WRONLY, S_IRWXU);
            if (fd_out < 0) 
            {
                handle_error("Fail to open a file.");
            }

            // カウントダウンタイマー開始
            // 60秒以内に、ダウンロードしなければコールバック関数が呼び出される。
            alarm(60);

            // 64バイトずつバッファに読み込んで、書き出す。
            do 
            {
                // 引数：１）ファイル記述子、２）読み込んだバイトを格納する変数、３）読み込むバイト数
                // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                n = read(socketd, buff, sizeof(buff));
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

            alarm(0);

            // ファイルを閉じる。
            close(fd_out);

            // サーバにACKを送信
            encode_pkt(bytes, ack, file_name, decoded_file_size);
            n = write(socketd, bytes, sizeof(bytes));
            if (n < 0)
            {
                handle_error("Fail to write a message.\n");
            }

            printf("%sのダウンロード完了\n", file_name);
            
            gettimeofday(&end_time, NULL);

            // RTTを計算
            double diff_sec = end_time.tv_sec - start_time.tv_sec;
            double diff_usec = end_time.tv_usec - start_time.tv_usec;
            double diff_time = diff_sec + diff_usec * 0.001 * 0.001; 
            printf("RTT = %f [s]\n", diff_time);

            putchar('\n');
        }

        /*--- アップロード ---*/
        else if (cmd == 2)
        {
            while (1)
            {
                // ユーザからのファイル名入力を待機
                printf("サーバにアップロードするファイル名: ");  scanf("%s", file_name);

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
            
            // The packet to be encoded
            printf("data = [%x, %s, %d]\n", data, file_name, file_size);

            // Encoding
            encode_pkt(bytes, data, file_name, file_size);

            // Print the bytes code.
            printf("E(data) =");
            for (int i = 0; i < 21; i++)
            {
                printf(" %x", bytes[i]);
            }
            putchar('\n');

            gettimeofday(&start_time, NULL);

            // パケットdataをサーバに送信
            n = write(socketd, bytes, sizeof(bytes));
            if (n < 0)
            {
                handle_error("Fail to write a message.\n");
            }

            // サーバからACKを受信
            n = read(socketd, bytes, sizeof(bytes));
            if (n < 0)
            {
                handle_error("Fail to read a message.\n");
            }

            decoded_file_size = 0;
            decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
            if (decoded_cmd == ack)
            {
                printf("ACKを受信(パケットdataの受信)\n");
                printf("%sをアップロード中\n", decoded_file_name);
            }

            // ファイルデータを送信（64バイトずつ）
            // ファイル１（コピー元）を読み込みモードで開く。
            int fd_in = open(file_name, O_RDONLY);
            if (fd_in < 0) 
            {
                handle_error("Fail to open a file.");
            }

            // カウントダウンタイマー開始
            // 60秒以内に、アップロードしなければコールバック関数が呼び出される。
            alarm(60);

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
            
                // 書き出し
                // 引数：１）ファイル記述子、２）書き出すバイトが格納された変数、３）書き込むバイト数
                // 戻り値は、実際に読み込んだバイト数。エラーの場合は-1が返される。
                n = write(socketd, buff, n);
                if (n < 0) 
                {
                    handle_error("Fail to read bytes from the input file.\n");
                }
                
                // コピー元ファイルを全て読みこめば、nの値が0となる。nが1以上でなければ、ループを抜ける。
            } 
            while (n > 0);

            alarm(0);

            // ファイルを閉じる。
            close(fd_in);

            // サーバからACKを受信
            n = read(socketd, bytes, sizeof(bytes));
            if (n < 0)
            {
                handle_error("Fail to read a message.\n");
            }

            decoded_file_size = 0;
            decode_pkt(bytes, &decoded_cmd, decoded_file_name, &decoded_file_size);
            if (decoded_cmd == ack)
            {
                printf("ACKを受信(ファイルアップロード完了)\n");
            }

            gettimeofday(&end_time, NULL);

            // RTTを計算
            double diff_sec = end_time.tv_sec - start_time.tv_sec;
            double diff_usec = end_time.tv_usec - start_time.tv_usec;
            double diff_time = diff_sec + diff_usec * 0.001 * 0.001; 
            printf("RTT = %f [s]\n", diff_time);

            putchar('\n');
        }

        /*--- 終了 ---*/
        else if (cmd == 3)
        {
            // Encoding
            encode_pkt(bytes, quit, file_name, file_size);
            // パケットquitをサーバに送信
            n = write(socketd, bytes, sizeof(bytes));
            if (n < 0)
            {
                handle_error("Fail to write a message.\n");
            }

            printf("サーバとの通信を終了します。\n");
            break;
        }

        /*--- それ以外 ---*/
        else
        {
            printf("適切な数字を入力してください。\n");
        }
    }

    // ソケットを閉じる。
    close(socketd);
}

/*
* 引数で受け取ったファイル名のファイルサイズを返す。s
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

// タイマーが切れた場合にコールバックされる関数
static void handler(int sig) 
{
    printf("\nThe timer expires.\n");
}
