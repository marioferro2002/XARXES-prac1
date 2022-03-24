#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>

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

struct config_info {
    unsigned char id[11];
    unsigned char elements[6][8];
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

struct socket_pck {
    int udp_socket;
    struct sockaddr_in udp_addr;

};


//Variables globals
int status = DISCONNECTED;
struct socket_pck sock;

//Funcions main
int check_debug_mode(int argc, char *argv[]);
void print_start_status(struct config_info info);
void show_status(unsigned char state);

//Funcions registre
struct config_info read_config_files(int argc, char *argv[], int debug_mode);
char *get_configname(int argc, char *argv[]);
struct config_pdu_udp client_to_pdu (struct config_info config);
void register_client_connection (struct config_info client, int debugmode);
void wait_reg_status(unsigned char status);
void udp_socket(struct config_info client);
void send_udp_package(struct config_info client, unsigned char type);
struct config_pdu_udp recv_pck_udp();
void wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp tcp_pck, struct config_info user);
struct config_pdu_udp ack_reg_packmaker (struct config_pdu_udp tcp_package, struct config_info user);

int main(int argc, char *argv[]) {

    int debug_status;
    debug_status = check_debug_mode(argc,argv);
    printf("mode debug: %i \n",debug_status);


    struct config_info user = read_config_files(argc, argv, debug_status);

    print_start_status(user);
    register_client_connection(user, debug_status);

}




void register_client_connection (struct config_info client, int debugmode){

    udp_socket(client);
    int unfinished_register_try = 0;


    while (unfinished_register_try < O){

        for(int i = 0; i < P; i++) {
            struct config_pdu_udp pck_recieved;
            send_udp_package(client,REG_REQ);
            status = WAIT_ACK_REG;
            wait_reg_status(status);
            pck_recieved = recv_pck_udp();

            if (pck_recieved.type == REG_ACK && status == WAIT_ACK_REG){
                wait_ack_reg_phase(REG_INFO, pck_recieved, client);
                return;
            }else if (pck_recieved.type == REG_NACK){
                show_status(REG_NACK);
            }else if (pck_recieved.type == REG_REJ){
                show_status(REG_REJ);
            }else if (pck_recieved.type == INFO_NACK){
                show_status(INFO_NACK);
            }
            else{
                printf("paquet inesperat");
                status = NOT_REGISTERED;
                unfinished_register_try++;
            }

        }
        sleep(5);

        unfinished_register_try++;
    }

}
void udp_socket(struct config_info client){
    struct sockaddr_in sck_addr_reg;
    struct hostent *ent;

    sock.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.udp_socket < 0){
        printf("Error al crear Socket %i", sock.udp_socket);
        exit(-1);
    }

    memset(&sck_addr_reg, 0, sizeof(struct sockaddr_in));
    sck_addr_reg.sin_addr.s_addr = htonl(INADDR_ANY);
    sck_addr_reg.sin_port = htonl(0);
    sck_addr_reg.sin_family = AF_INET;


    if( bind(sock.udp_socket,(struct sockaddr *)&sck_addr_reg, sizeof(sck_addr_reg)) < 0 ){
        printf("connexio dolenta");
        exit(-1);
    }


    ent = gethostbyname((const char*) client.server);
    if(!ent)
    {
        printf("Error! No trobat: %s \n",client.server);
        exit(-1);
    }

    memset(&sock.udp_addr, 0, sizeof(struct sockaddr_in));
    sock.udp_addr.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    sock.udp_addr.sin_port = htons(atoi((const char *)&client.server_udp ));
    sock.udp_addr.sin_family = AF_INET;


}

void send_udp_package(struct config_info client, unsigned char type){
    struct config_pdu_udp reg_str = client_to_pdu(client);
    reg_str.type = type;
    int bytes_send = 0;


    bytes_send = sendto(sock.udp_socket,(void *)&reg_str,sizeof(reg_str), 0, (const struct sockaddr*)&sock.udp_addr, sizeof(sock.udp_addr));

    if(bytes_send != 84){
        printf("tamañ no coincideix\n");
    }

}

struct config_pdu_udp recv_pck_udp(){
    struct config_pdu_udp reg_server;
    int a = 0;
    a = recvfrom(sock.udp_socket, (void *)&reg_server, sizeof(struct config_pdu_udp), 0, (struct sockaddr*) 0,(socklen_t *) 0);
    if(a < 0){
        printf("No s'ha rebut cap paquet per UDP");
        exit(1);
    }
    return reg_server;
}

void wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp tcp_pck, struct config_info user){

    struct sockaddr_in addr_tcp_port = sock.udp_addr;
    unsigned char server_port[6];
    int bytes_send = 0;

    tcp_pck.type = REG_INFO;
    show_status(tcp_pck.type);

    //Creem copia de la addreça i canviem port
    strcpy((char *)server_port, tcp_pck.dades);
    tcp_pck = ack_reg_packmaker(tcp_pck, user);
    addr_tcp_port.sin_port = htons(atoi((const char*)&server_port));


    bytes_send = sendto(sock.udp_socket,(void *)&tcp_pck,sizeof(tcp_pck), 0, (const struct sockaddr*)&addr_tcp_port, sizeof(addr_tcp_port));

    if (bytes_send != 84){
        exit(1);
    }
    struct config_pdu_udp info_ack_packet;

    sleep(2 * T);


    show_status(info_ack_packet.type);
}


struct config_pdu_udp ack_reg_packmaker (struct config_pdu_udp tcp_package, struct config_info user){
    strcpy((char *)&tcp_package.id_transm, (const char*)&user.id);
    strcpy((char *)&tcp_package.dades[0], (const char*)&user.local_TCP);
    strcpy((char *)&tcp_package.dades[4], (const char*) ",");
    int i = 0;
    int pos = 5;

    while (strcmp((char *)&user.elements[i],"NULL") != 0 && i < 5){
        strcpy((char *)&tcp_package.dades[pos], (const char*)&user.elements[i]);

        if(strcmp((char *)&user.elements[i + 1],"NULL") != 0 && i < 4){
            strcpy((char *)&tcp_package.dades[pos + 7], (const char *) ";");
        }
        pos += 8;
        i++;
    }


    return tcp_package;
}

















/*------------------FUNCIONS GENERALS----------------------------*/

struct config_pdu_udp client_to_pdu (struct config_info config){

    struct config_pdu_udp reg_pack;

    strcpy((char *) reg_pack.id_transm,(const char *) config.id);
    strcpy((char *) reg_pack.id_comm,"0000000000");
    strcpy(reg_pack.dades, "");

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

void show_status(unsigned char state) {

    switch(state){
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
        case 0xa0:
            printf("REG_REQ \n");
            break;
        case 0xa1:
            printf("REG_ACK \n");
            break;
        case 0xa2:
            printf("REG_NACK \n");
            break;
        case 0xa3:
            printf("REG_REJ\n");
            break;
        case 0xa4:
            printf("REG_INFO\n");
            break;
        case 0xa5:
            printf("INFO_ACK\n");
            break;
        case 0xa6:
            printf("INFO_NACK\n");
            break;
        case 0xa7:
            printf("INFO_REJ\n");
            break;
    }
}

void wait_reg_status(unsigned char status){
    int h, m, s;
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);
    h = tm_struct -> tm_hour;
    m = tm_struct -> tm_min;
    s = tm_struct -> tm_sec;

    printf("%i:%i:%i MSG: Dispositiu passa a l'estat: ", h, m, s);
    show_status(status);

}








