#include <stdio.h>
#include <string.h>

int check_debug(int argc, char *argv[]);

struct config_info {
    unsigned char id[10];
    unsigned char elements[16][6];
    unsigned char local_TCP[5];
    unsigned char server[7];
    unsigned char server_udp[5];

};

int main(int argc, char *argv[]) {

    int debug_status = check_debug(argc, argv);
}

struct config_info read_config_files (int arg, int argv[], int debug_mode){
    struct config_info client;
    if(debug_mode == 1){
        printf("Configuració del client guardada !");
    }
    return client;
}

int check_debug_mode(int argc,char *argv[]){
    for(int i = 1; i < argc;i++){
        if(strcmp(argv[1],"-d") == 0){
            return 1;
        }
    }
    return 0;
}