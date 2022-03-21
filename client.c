#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_CHAR 51 //El tamany del array es tan gran a causa de la linea d'elements
#define T 1
#define U 2
#define N 8
#define O 3
#define P 2
#define Q 4

//Tipus de paquets

#define REG_REQ 0xa0
#define REG_ACK 0xa1
#define REG_NACK 0xa2
#define REG_REJ 0xa3
#define REG_INFO 0xa4
#define INFO_ACK 0xa5
#define INFO_NACK 0xa6
#define INFO_REJ 0xa7

//Estat del client/servidor

#define DISCONNECTED 0xf0
#define NOT_REGISTERED 0xf1
#define WAIT_ACK_REG 0xf2
#define WAIT_INFO 0xf3
#define WAIT_ACK_INFO 0xf4
#define REGISTERED 0xf5
#define SEND_ALIVE 0xf6
int status = DISCONNECTED;

struct config_info {
    unsigned char id[11];
    unsigned char elements[6][16];
    unsigned char local_TCP[6];
    unsigned char server[13];
    unsigned char server_udp[6];

};

struct config_pdu_udp {
    unsigned char type;
    char id_transm[11];
    char id_comm[11];
    char dades[61];
};


//Funcions main
int check_debug_mode(int argc, char *argv[]);
void print_start_status(struct config_info info);
void show_status(int status);

//Funcions registre
struct config_info read_config_files(int argc, char *argv[], int debug_mode);
char *get_configname(int argc, char *argv[]);
struct config_pdu_udp client_to_pdu (struct config_info config);
int register_client_connection (struct config_info client, int debugmode);
unsigned char* create_pack_udp(struct config_pdu_udp udp_pack, char type);
void wait_reg_status(int status);



int main(int argc, char *argv[]) {

    int debug_status;
    debug_status = check_debug_mode(argc,argv);
    printf("mode debug: %i \n",debug_status);


    struct config_info user = read_config_files(argc, argv, debug_status);
    print_start_status(user);
    int connect = register_client_connection(user, debug_status);

}




int register_client_connection (struct config_info client, int debugmode){

    int init_socket;
    //Creem socket
    init_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (init_socket < 0){
        printf("Error al crear Socket %i", init_socket);
        exit(-1);
    }

    //Creem constructors y variables necessaries per al bind i comunicació amb el server
    struct sockaddr_in sck_addr_reg;
    sck_addr_reg.sin_addr.s_addr = htonl(0);
    sck_addr_reg.sin_port = htonl(0);
    sck_addr_reg.sin_family = AF_INET;


    if( bind(init_socket,(struct sockaddr*)&sck_addr_reg, sizeof(struct sockaddr)) != 0 ){
        printf("connexio dolenta");
        exit(-1);
    }
    //Linkem adreça del servidor
    sck_addr_reg.sin_addr.s_addr = htonl();
    sck_addr_reg.sin_port = htons(client.server_udp);
    sck_addr_reg.sin_family = AF_INET;

    //Preparem tos els paquets per al registre i començem amb l'estat WAIT_REG

    struct config_pdu_udp reg_str = client_to_pdu(client);
    status = NOT_REGISTERED;
    wait_reg_status(status);
    return 0;

    //sendto(init_socket, reg_str, sizeof(reg_str), )
}






















/*------------------FUNCIONS GENERALS----------------------------*/

struct config_pdu_udp client_to_pdu (struct config_info config){

    struct config_pdu_udp reg_pack;
    strcpy((char *) reg_pack.id_transm,(const char *) config.id);
    strcpy((char *) reg_pack.id_comm,"0000000000");
    strcpy(reg_pack.dades, "sdddddd");

    return reg_pack;
}

struct config_info read_config_files(int argc, char *argv[], int debug_mode) {
    struct config_info client;
    char *name_config = get_configname(argc, argv);
    char info[MAX_CHAR];
    FILE *file;
    file = fopen(name_config, "r");
    char *cpy_token;

    //ID
    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    strcpy((char *) client.id, cpy_token);

    //ELEMENTS
    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    cpy_token = strtok(cpy_token, ";");

    int i = 0;
    while (cpy_token != NULL){

        strcpy((char *) client.elements[i], cpy_token);
        cpy_token = strtok(NULL, ";");
        i++;
    }
    for(int x = i; x < 6; x++){
        strcpy((char *) client.elements[i], "NULL");
    }




    //local_tcp

    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    strcpy((char *) client.local_TCP,cpy_token);

    //server

    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    strcpy((char *) client.server, cpy_token);


    //server-UDP

    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    strcpy((char *) client.server_udp, cpy_token);




    if(debug_mode == 1){
        //printf("Configuració del client guardada!!\n Id: %s \n Elements: %s ...\n Local_TCP: %s \n Server: %s \n Server_UDP: %s \n", client.id, client.elements[1], client.local_TCP, client.server, client.server_udp);
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
void print_start_status(struct config_info info) {
    printf("********************* DADES DISPOSITIU ***********************\n");
    printf("Identificador: %s\n", info.id);
    printf("Estat: DISCONNECTED\n\n");
    printf(" Param      valor\n");
    printf(" -----      --------\n");
    int i = 0;

   while(strcmp((char *)info.elements[i],"NULL") != 0){
           printf("%s       None\n", info.elements[i]);
       i++;
   }
    printf("***************************************************************\n");


}

void show_status(int status) {

    switch(status){
        case 0xf0:
            printf("DISCONNECTED\n");
            break;
        case 0xf1:
            printf("NOT_REGISTERED\n");
            break;
        case 0xf2:
            printf("WAIT_ACK_REG\n");
            break;
        case 0xf3:
            printf("WAIT_INFO\n");
            break;
        case 0xf4:
            printf("WAIT_ACK_INFO\n");
            break;
        case 0xf5:
            printf("REGISTERED\n");
            break;
        case 0xf6:
            printf("SEND_ALIVE\n");
            break;
    }
}

void wait_reg_status(int status){
    int h, m, s;
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);
    h = tm_struct -> tm_hour;
    m = tm_struct -> tm_min;
    s = tm_struct -> tm_sec;

    printf("%i:%i:%i MSG: Dispositiu passa a l'estat: ", h, m, s);
    show_status(status);

}








