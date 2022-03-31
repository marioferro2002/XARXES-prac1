#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>
#include <threads.h>
//Doc General Defines

#define MAX_CHAR 51
#define T 1
#define U 2
#define N 8
#define O 3
#define P 2
#define Q 4
#define V 2
#define R 2
#define S 2


//Packet_type

#define REG_REQ 0xa0
#define REG_ACK 0xa1
#define REG_NACK 0xa2
#define REG_REJ 0xa3
#define REG_INFO 0xa4
#define INFO_ACK 0xa5
#define INFO_NACK 0xa6
#define INFO_REJ 0xa7

//Client-server state

#define DISCONNECTED 0xf0
#define NOT_REGISTERED 0xf1
#define WAIT_ACK_REG 0xf2
#define WAIT_INFO 0xf3
#define WAIT_ACK_INFO 0xf4
#define REGISTERED 0xf5
#define SEND_ALIVE 0xf6
#define ALIVE  0xb0
#define ALIVE_NACK 0xb1
#define ALIVE_REJ 0xb2

/*+++++++++++++++++++++STRUCTS+++++++++++++++++++++*/
struct config_info {
    unsigned char id[11];
    unsigned char elements[6][8];
    unsigned char valor_elements[6][16];
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

struct server_info {
    char id_transm[11];
    char id_comm[11];
    char ip[9];
    unsigned char port[6];
};

struct socket_pck {
    int udp_socket;
    int tcp_socket;
    int tcp_port;
    struct sockaddr_in udp_addr;
    struct sockaddr_in tcp_addr;
    struct timeval udp_time_out;
};


/*+++++++++++++++++++++GLOBAL-FUNCTIONS+++++++++++++++++++++*/
int check_debug_mode(int argc, char *argv[]);
void print_start_status(struct config_info info);
void show_status(unsigned char state);
char *get_configname(int argc, char *argv[]);
struct config_info read_config_files(int argc, char *argv[], int debug_mode);

/*+++++++++++++++++++++REGISTER-FUNCTIONS+++++++++++++++++++++*/
//Main register functions
void register_client_connection (struct config_info client, int debugmode);
void udp_socket(struct config_info client);
struct config_pdu_udp client_to_pdu (struct config_info config);
void save_server_info(struct config_pdu_udp server_pck);
//send-recieve functions
void send_udp_package(unsigned char type, struct config_pdu_udp packet_to_send);
struct config_pdu_udp recv_pck_udp(int time_out);
//ack-reg phase
struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp tcp_pck, struct config_info user);
struct config_pdu_udp ack_reg_packmaker (struct config_pdu_udp tcp_package, struct config_info user);
//Prints
void wait_reg_status(unsigned char status);
//Error control & sleeps
bool valid_server(struct config_pdu_udp server_pack);
void wait_after_send_package(int package_order);

/*+++++++++++++++++++++ALIVE-STATE-FUNCTIONS+++++++++++++++++++++*/
void stay_alive(struct  config_info client_info);
void tcp_socket(struct config_info client_info);
void periodic_communication(struct config_info client);
char *read_command();
void command_treatment();
bool validate_alive(struct config_pdu_udp recieved_packet);
void set_element_(char *buffer);




//Variables globals
int status = DISCONNECTED;
struct socket_pck sock;
struct server_info inf_server;
struct sockaddr_in recieved_pack_addr;
pthread_t tid = (pthread_t) NULL;
struct config_info user;

int main(int argc, char *argv[]) {

    int debug_status;
    user = read_config_files(argc, argv, debug_status);

    debug_status = check_debug_mode(argc,argv);
    printf("mode debug: %i \n",debug_status);
    print_start_status(user);
    //fase register
    status = NOT_REGISTERED;
    register_client_connection(user, debug_status);
    //fase alive
    thrd_create(&tid, (thrd_start_t) command_treatment, 0);
    stay_alive(user);
}


/*------------------------------FASE REGISTER---------------------------------------------------*/

void register_client_connection (struct config_info client, int debugmode){

    udp_socket(client);
    int unfinished_register_try = 0;

    while (unfinished_register_try < O && status != REGISTERED){
        printf("Intent de registre: %i \n", unfinished_register_try + 1);
        wait_reg_status(status);
        status = WAIT_ACK_REG;
        wait_reg_status(status);

        for(int i = 0; i < N; i++) {

            struct config_pdu_udp pck_recieved;
            send_udp_package(REG_REQ, client_to_pdu(client));
            pck_recieved = recv_pck_udp(T);
            save_server_info(pck_recieved);

            if (pck_recieved.type == REG_ACK && status == WAIT_ACK_REG){
                pck_recieved = wait_ack_reg_phase(REG_INFO, pck_recieved, client);
            }
            if (pck_recieved.type == REG_NACK){
                status = NOT_REGISTERED;
                wait_reg_status(status);
            }
            if (pck_recieved.type == REG_REJ){
                status = NOT_REGISTERED;
                wait_reg_status(status);
                break;
            }
            if (pck_recieved.type == INFO_ACK && status == WAIT_ACK_INFO && valid_server(pck_recieved)){
                status = REGISTERED;
                wait_reg_status(status);
                sock.tcp_port = atoi(pck_recieved.dades);
                return;
            }
            if (pck_recieved.type == INFO_NACK && status == WAIT_ACK_INFO && valid_server(pck_recieved)){
                printf("Motiu de falla del registre: %s",pck_recieved.dades);
                status = NOT_REGISTERED;
                wait_reg_status(status);
            }else{
                status = NOT_REGISTERED;
            }
            wait_after_send_package(i);
        }
        sleep(U);
        unfinished_register_try++;
    }
    printf("CLIENT NOT REGISTERED \n");
    exit(1);

}
/*-----------------------------------------SOCKETS & PACKETS STATES---------------------------*/
void udp_socket(struct config_info client){
    struct sockaddr_in client_addr;
    struct hostent *ent;

    sock.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.udp_socket < 0){
        printf("Error al crear Socket %i", sock.udp_socket);
        exit(-1);
    }

    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htonl(0);
    client_addr.sin_family = AF_INET;

    if(bind(sock.udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0 ){
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

struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp tcp_pck, struct config_info user){

    struct sockaddr_in addr_tcp_port = sock.udp_addr;
    unsigned char server_port[6];
    int bytes_send = 0;
    struct config_pdu_udp info_ack_packet;

    tcp_pck.type = REG_INFO;

    //Creem copia de la addreça i canviem port
    strcpy((char *)server_port, tcp_pck.dades);
    tcp_pck = ack_reg_packmaker(tcp_pck, user);
    addr_tcp_port.sin_port = htons(atoi((const char*)&server_port));


    sendto(sock.udp_socket,(void *)&tcp_pck,sizeof(tcp_pck), 0, (const struct sockaddr*)&addr_tcp_port, sizeof(addr_tcp_port));


    status = WAIT_ACK_INFO;
    wait_reg_status(status);
    info_ack_packet = recv_pck_udp(2*T);

    return info_ack_packet;
}


/*--------------------------------------SEND & RECIEVE PACKETS UDP-----------------------------------*/

void send_udp_package(unsigned char type, struct config_pdu_udp packet_to_send){
    struct config_pdu_udp reg_str = packet_to_send;
    int bytes_send = 0;

    reg_str.type = type;

    bytes_send = sendto(sock.udp_socket,(void *)&reg_str,sizeof(reg_str), 0, (const struct sockaddr*)&sock.udp_addr, sizeof(sock.udp_addr));
    if(bytes_send != 84){
        printf("tamañ no coincideix\n");
    }

}

struct config_pdu_udp recv_pck_udp(int time_out) {
    struct config_pdu_udp reg_server;
    int a = 0;
    fd_set writefds;

    FD_ZERO(&writefds);
    FD_SET(sock.udp_socket, &writefds);
    sock.udp_time_out.tv_sec = time_out;


    if (select(sock.udp_socket + 1, &writefds, NULL, NULL, &sock.udp_time_out) > 0) {

        recvfrom(sock.udp_socket, (void *) &reg_server, sizeof(struct config_pdu_udp), 0,
                 (struct sockaddr *) &recieved_pack_addr, (socklen_t *) &recieved_pack_addr);

    }
    return reg_server;

}
struct config_pdu_udp ack_reg_packmaker (struct config_pdu_udp tcp_package, struct config_info user){
    int i = 0;
    int pos = 5;

    strcpy((char *)&tcp_package.id_transm, (const char*)&user.id);
    strcpy((char *)&tcp_package.dades[0], (const char*)&user.local_TCP);
    strcpy((char *)&tcp_package.dades[4], (const char*) ",");

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


/*--------------------------------------------ERROR CONTROLS & BOOLEANS-------------------*/

bool valid_server(struct config_pdu_udp server_pack){

    if(strcmp(inf_server.id_comm, server_pack.id_comm) == 0 && strcmp(inf_server.id_transm, server_pack.id_transm) == 0 && sock.udp_addr.sin_addr.s_addr == recieved_pack_addr.sin_addr.s_addr){
        return true;
    }else{
        return false;
    }
}
void save_server_info(struct config_pdu_udp server_pck){

    strcpy(inf_server.id_transm, server_pck.id_transm);
    strcpy(inf_server.id_comm, server_pck.id_comm);
    strcpy(inf_server.port, server_pck.dades);

}

void wait_after_send_package(int package_order){
    if(package_order > P ){

        if((T + (T *(package_order - 2))) < T * Q ){
            sleep(T + (T *(package_order - 2)) );
        }else{
            sleep(Q * T);
        }

    } else{
        sleep(T);
    }

}
/*------------------SENDA ALIVE PHASE----------------------------*/

void stay_alive(struct  config_info client_info){
    periodic_communication(client_info);
}
void periodic_communication(struct config_info client){

    struct config_pdu_udp alive_packet;
    int fake_alive_packets = 0;

    alive_packet = client_to_pdu(client);
    strcpy(alive_packet.id_comm, inf_server.id_comm);
    bool first_packet_alive = true;

        while(1){
            struct config_pdu_udp recieved_packet;
            strcpy(alive_packet.id_comm, inf_server.id_comm);
            send_udp_package(ALIVE, alive_packet);
            recieved_packet = recv_pck_udp(R);

            if(!validate_alive(recieved_packet)){
                printf("paquets no valids: %i\n", fake_alive_packets + 1);
                if(first_packet_alive){
                    status = NOT_REGISTERED;
                    wait_reg_status(status);
                    register_client_connection(client, 0);
                }else if(fake_alive_packets == S){
                    fake_alive_packets = 0;
                    status = NOT_REGISTERED;
                    wait_reg_status(status);
                    register_client_connection(client, 0);
                }else{
                    fake_alive_packets++;
                }
            }
            if(validate_alive(recieved_packet) && first_packet_alive){
                printf("primer ALIVE rebut\n");
                first_packet_alive = false;
                status = SEND_ALIVE;
                wait_reg_status(status);
                tcp_socket(client);
            }
            sleep(V);
        }

}

void command_treatment(){

    while(1){
        char *command = read_command();
        if(strstr(command, "status") == command){
            print_start_status(user);
        }else if(strstr(command, "set") == command){
            set_element_(command);
        }else if(strstr(command, "send") == command){
            printf("send\n");
        }else{
            printf("hello");
        }
    }
}


char *read_command() {

    char *command = malloc(MAX_CHAR);
    char buffer[MAX_CHAR];

    if (fgets(buffer, MAX_CHAR, stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
    strcpy(command, buffer);
    return command;
}
bool validate_alive(struct config_pdu_udp recieved_packet){

    if (valid_server(recieved_packet) && strcmp(recieved_packet.dades, (const char*)&user.id) == 0 && recieved_packet.type == ALIVE){
        return  true;
    } else{
        show_status(ALIVE);
        printf("%s\n",recieved_packet.dades);
        return false;
    }
}


void set_element_(char *buffer){
    char *cpy_token, *element;
    char element_value[16];


    buffer[strlen(buffer) - 1] = '\0';
    //Obtain element
    cpy_token = strtok(buffer, " ");
    cpy_token = strtok(NULL, " ");
    if(cpy_token == NULL){
        printf("Us comanda: set <identificador_element> <nou_valor> \n");
        return;
    }
    element = cpy_token;

    //Obtain value
    cpy_token = strtok(NULL, " ");
    if(cpy_token == NULL){
        printf("Us comanda: set <identificador_element> <nou_valor> \n");
        return;
    }
    strncpy(element_value,cpy_token,16);
    printf("%s\n", element_value);
    printf("%s\n", element);

    for(int i = 0; i < 6;i++){

        if(strcmp((const char *)&user.elements[i], element) == 0){
            printf("entra\n");
            strcpy((char *)&user.valor_elements[i], element_value);
            printf("%s", user.valor_elements[i]);
            return;
        }

    }
    printf("Element no trobat \n");
}











/*-----------------------------------------------------------TCP????----------------------------------*/
void tcp_socket(struct config_info client_info){
    struct hostent *ent;
    ent = gethostbyname((char *)&client_info.server);

    if(!ent){
        printf("Server no trobat creant socket TCP\n");
        exit(1);
    }

    sock.tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(sock.tcp_socket < 0){
        printf("Error al crear Socket tcp \n");
        exit(1);
    }

    memset(&sock.tcp_addr, 0, sizeof(struct sockaddr_in));
    sock.tcp_addr.sin_family = AF_INET;
    sock.tcp_addr.sin_addr.s_addr = (((struct in_addr *) ent->h_addr_list[0])->s_addr);
    sock.tcp_addr.sin_port = htons(sock.tcp_port);

    if(connect(sock.tcp_socket, (struct sockaddr *)&sock.tcp_addr, sizeof(sock.tcp_addr)) < 0){
        printf("Error al connectar amb server via TCP\n");
        exit(1);
    }

}






/*------------------GENERAL FUNCTIONS----------------------------*/

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
    show_status(status);
    printf(" Param      valor\n");
    printf(" -----      --------\n");
    int i = 0;

   while(strcmp((char *)info.elements[i],"NULL") != 0){
           printf("%s       %s\n", info.elements[i],info.valor_elements[i]);
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
        case SEND_ALIVE:
            printf("SEND_ALIVE\n");
            break;
        case ALIVE:
            printf("ALIVE\n");
            break;
        case ALIVE_NACK:
            printf("ALIVE_NACK\n");
            break;
        case ALIVE_REJ:
            printf("ALIVE_REJ\n");
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








