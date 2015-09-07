#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

uint8_t key[16];

#define MAX_BLOCKS 20
uint8_t data[MAX_BLOCKS * 16];

uint8_t round_key[16];

typedef uint8_t state_t[4][4];
state_t* under_process;

const uint8_t sbox[256] = {
   0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
   0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
   0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
   0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
   0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
   0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
   0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
   0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
   0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
   0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
   0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
   0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
   0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
   0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
   0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
   0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

const uint8_t rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

void gen_next_round_key(uint8_t round) {
    int i;
    if (round == 0) {
        for(i = 0; i < 16; ++i) {
            round_key[i] = key[i];
        }
    } else {
        round_key[0] ^= sbox[round_key[13]] ^ rcon[round];
        round_key[1] ^= sbox[round_key[14]];
        round_key[2] ^= sbox[round_key[15]];
        round_key[3] ^= sbox[round_key[12]];
        for (i = 4; i < 16; ++i) {
            round_key[i] ^= round_key[i - 4];
        }

    }
}

void add_round_key(uint8_t round) {
    uint8_t i, j;
    gen_next_round_key(round);
    for(i = 0; i < 4;++i) {
        for(j = 0; j < 4; ++j) {
            (*under_process)[i][j] ^= round_key[i * 4 + j];
        }
    }
}

void sub_bytes(void) {
    uint8_t i, j;
    for(i = 0; i < 4; ++i) {
        for(j = 0; j < 4; ++j) {
            (*under_process)[j][i] = sbox[(*under_process)[j][i]];
        }
    }
}

void shift_rows(void) {
    uint8_t temp;

    temp = (*under_process)[0][1];
    (*under_process)[0][1] = (*under_process)[1][1];
    (*under_process)[1][1] = (*under_process)[2][1];
    (*under_process)[2][1] = (*under_process)[3][1];
    (*under_process)[3][1] = temp;

    temp = (*under_process)[0][2];
    (*under_process)[0][2] = (*under_process)[2][2];
    (*under_process)[2][2] = temp;

    temp = (*under_process)[1][2];
    (*under_process)[1][2] = (*under_process)[3][2];
    (*under_process)[3][2] = temp;

    temp = (*under_process)[0][3];
    (*under_process)[0][3] = (*under_process)[3][3];
    (*under_process)[3][3] = (*under_process)[2][3];
    (*under_process)[2][3] = (*under_process)[1][3];
    (*under_process)[1][3] = temp;
}

uint8_t xtime(uint8_t x) {
    return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

void mix_columns(void) {
    uint8_t i;
    uint8_t tmp,tm,t;
    for(i = 0; i < 4; ++i) {
        t   = (*under_process)[i][0];
        tmp = (*under_process)[i][0] ^ (*under_process)[i][1] ^ (*under_process)[i][2] ^ (*under_process)[i][3];
        tm  = (*under_process)[i][0] ^ (*under_process)[i][1]; tm = xtime(tm);  (*under_process)[i][0] ^= tm ^ tmp;
        tm  = (*under_process)[i][1] ^ (*under_process)[i][2]; tm = xtime(tm);  (*under_process)[i][1] ^= tm ^ tmp;
        tm  = (*under_process)[i][2] ^ (*under_process)[i][3]; tm = xtime(tm);  (*under_process)[i][2] ^= tm ^ tmp;
        tm  = (*under_process)[i][3] ^ t;  tm = xtime(tm);  (*under_process)[i][3] ^= tm ^ tmp;
    }
}

void encrypt_one_block(void) {
    uint8_t round;
    add_round_key(0);
    for(round = 1; round < 10; ++round) {
        sub_bytes();
        shift_rows();
        mix_columns();
        add_round_key(round);
    }
    sub_bytes();
    shift_rows();
    add_round_key(10);
}

void load_key(void) {
    FILE* key_file = fopen("key.bin", "rb");
    if (!key_file) {
        perror("opening key file");
        exit(1);
    }

    if (1 != fread(key, 16, 1, key_file)) {
        perror("reading key");
        exit(1);
    }

    fclose(key_file);
}

void error(const char* message) {
    fprintf(stderr, "ERROR\n%s\n", message);
    fflush(stderr);
    exit(1);
}

void check_filename(char* filename) {
    int filename_len, i;
    filename_len = strlen(filename);
    if (filename_len > 0 && filename[filename_len - 1] == '\n') {
        --filename_len;
        filename[filename_len] = '\0';
    }

    if (filename_len <= 0) {
        error("empty filename");
    }

    for (i = 0; i < filename_len; ++i) {
        if (filename[i] == '\\' || filename[i] == '/' || filename[i] < 32) {
            error("oy vey");
        }
    }

    if (!strcmp(filename, "..") || !strcmp(filename, ".")) {
            error("oy vey");
    }

    for (i = 0; i < filename_len; ++i) {
        if (!  ( ((filename[i] >= 'a') && (filename[i] <= 'z')) ||
                 ((filename[i] >= 'A') && (filename[i] <= 'Z')) ||
                 ((filename[i] >= '0') && (filename[i] <= '9')) ||
                (filename[i] == '_') || (filename[i] == '-') || (filename[i] == '.'))) {
            error("bad characters in filename");
        }
    }
}

void process_client() {
    char* filename = NULL;
    char* data_size_buf = NULL;
    int data_size, block_count, block_index, i, j;
    size_t buffer_len;
    FILE* output;

    if (-1 == getline(&filename, &buffer_len, stdin)) {
        error("cannot read filename");
    }

    check_filename(filename);

    if (-1 == getline(&data_size_buf, &buffer_len, stdin)) {
        error("cannot read data size");
    }

    if (1 != sscanf(data_size_buf, "%d\n", &data_size)) {
        error("cannot extract data size");
    }

    if (data_size < 0) {
        error("invalid data size");
    }

    block_count = data_size / 16;

    if (block_count > MAX_BLOCKS) {
        error("data too large");
    }

    for (block_index = 0; block_index < block_count; ++block_index) {
        under_process = (state_t*) &data[block_index * 16];

        if (fread(under_process, 1, 16, stdin) != 16) {
            error("cannot read data");
        }

        encrypt_one_block();
    }

    output = fopen(filename, "wb");
    if (!output) {
        error("cannot open output file for writing");
    }

    if (data_size != fwrite(data, 1, data_size, output)) {
        error("cannot write data");
    }

    fclose(output);

    printf("OK\n%s\n", filename);
}

void set_sigchild_handler(void) {
	struct sigaction act;
	memset (&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGCHLD, &act, 0)) {
		perror("sigaction");
		exit(1);
	}
}

int create_server_socket(void) {
    struct sockaddr_in serv_addr;
    int true_ = 1, server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
      perror("error creating server socket");
      exit(1);
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(true_)) == -1) {
        perror("error in setsockopt");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serv_addr.sin_port = htons(3456);
    if (bind(server_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("error in bind");
        exit(1);
    }

    listen(server_socket, 5);
    return server_socket;
}

void set_socket_timeout(int socket) {
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,  sizeof(timeout));
}

int main(int argc, char **argv) {
    int server_socket, client_socket;
    int pid;

    if (argc != 2) {
        printf("usage: %s [temporary files directory]\n", argv[0]);
        exit(1);
    }

    set_sigchild_handler();
    load_key();

    if (-1 == chdir(argv[1])) {
        perror("error in chdir");
        exit(1);
    }

    server_socket = create_server_socket();

    while (1) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("error in accept");
        } else {
            pid = fork();
            if (pid < 0) {
                perror("error in fork");
                close(client_socket);
                continue;
            }

            if (pid == 0) {
                close(server_socket);
                set_socket_timeout(client_socket);
                dup2(client_socket, 0);
                dup2(client_socket, 1);
                dup2(client_socket, 2);
                close(client_socket);
                process_client();
                exit(0);
            } else {
                close(client_socket);
            }
        }
   }
}
