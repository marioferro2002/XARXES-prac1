#include <stdio.h>
#include <string.h>
#define MAX_CHAR 51 //El tamany del array es tan gran a causa de la linea d'elements


//Funcions main
int check_debug_mode(int argc, char *argv[]);

//Funcions registre
struct config_info read_config_files(int argc, char *argv[], int debug_mode);
char *get_configname(int argc, char *argv[]);
void get_elements(char *elements);

struct config_info {
    unsigned char id[10];
    unsigned char elements[16][6];
    unsigned char local_TCP[5];
    unsigned char server[7];
    unsigned char server_udp[5];

};

int main(int argc, char *argv[]) {

    int debug_status;
    debug_status = check_debug_mode(argc,argv);
    printf("mode debug: %i \n",debug_status);

    struct config_info user = read_config_files(argc, argv, debug_status);

}

struct config_info read_config_files(int argc, char *argv[], int debug_mode){
    struct config_info client;
    char *name_config = get_configname(argc,argv);
    char info[MAX_CHAR];
    FILE *file;
    file = fopen(name_config,"r");
    char *cpy_token;

    //ID
    fgets(info, MAX_CHAR,file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info," ");
    cpy_token = strtok(NULL,"= ");
    strcpy((char*)client.id,cpy_token);
    printf("%s \n", cpy_token);

    //ELEMENTS
    fgets(info, MAX_CHAR,file);
    printf("%s \n", info);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info," ");
    cpy_token = strtok(NULL,"= ");

    get_elements(cpy_token);




    if(debug_mode == 1){
        printf("Configuració del client guardada!!\n");
    }
    return client;
}

int check_debug_mode(int argc,char *argv[]){
    for(int i = 1; i < argc;i++){
        if(strcmp(argv[i], "-d") == 0){
            return 1;
        }
    }
    return 0;
}
char *get_configname(int argc, char *argv[]){
    for(int i = 1; i < argc;i++){
        if(strcmp(argv[i], "-c") == 0){
            return argv[i+1];
        }
    }
    return "client.cfg";
}
void get_elements(char *elements){
    char *cpy_token = " ";
    int i = 0;
    while(strcmp(cpy_token, "\0" ) != 0){
        cpy_token = strtok(elements,";");

        printf("%s \n", cpy_token);
        break;
    }
}