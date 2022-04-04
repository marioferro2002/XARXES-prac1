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
#include <sys/select.h>
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
#define M 3


//Packet_type

#define REG_REQ 0xa0
#define REG_ACK 0xa1
#define REG_NACK 0xa2
#define REG_REJ 0xa3
#define REG_INFO 0xa4
#define INFO_ACK 0xa5
#define INFO_NACK 0xa6
#define INFO_REJ 0xa7
#define ALIVE_NACK 0xb1
#define ALIVE_REJ 0xb2
#define SEND_DATA 0xc0
#define DATA_ACK 0xc1
#define DATA_NACK 0xc2
#define DATA_REJ 0xc3
#define SET_DATA 0xc4
#define GET_DATA 0xc5




//Client-server state

#define DISCONNECTED 0xf0
#define NOT_REGISTERED 0xf1
#define WAIT_ACK_REG 0xf2
#define WAIT_INFO 0xf3
#define WAIT_ACK_INFO 0xf4
#define REGISTERED 0xf5
#define SEND_ALIVE 0xf6
#define ALIVE  0xb0


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
    int tcp_socket_recv;
    int tcp_port;
    struct sockaddr_in udp_addr;
    struct sockaddr_in tcp_addr;
    struct sockaddr_in tcp_addr_recv;
    struct timeval recv_timeout;
};

struct pdu_tcp_pack{
    unsigned char type;
    char id_transm[11];
    char id_comm[11];
    char element[8];
    char valor[16];
    char info[80];
};

/*+++++++++++++++++++++REGISTER-FUNCTIONS+++++++++++++++++++++*/
void register_client_connection ();

/*+++++++++++++++++++++SOCKETS TCP & UDP+++++++++++++++++++++*/

void tcp_socket();
void tcp_socket_recv();
void udp_socket();

/*+++++++++++++++++++++ALIVE-STATE-FUNCTIONS+++++++++++++++++++++*/

void stay_alive(struct  config_info client_info);
void periodic_communication(struct config_info client);
void periodic_local_tcp_comms();

/*+++++++++++++++++++++SEND & RECEIVED UDP/TCP+++++++++++++++++++++*/

void send_udp_package(unsigned char type, struct config_pdu_udp packet_to_send);
struct config_pdu_udp recv_pck_udp(int time_out);
void send_package_via_tcp_to_server(struct pdu_tcp_pack send_pck, int socket);
void receive_tcp_pck_from_server(int time_out, int socket);

/*+++++++++++++++++++++PACKET CONTROL+++++++++++++++++++++*/

struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp reg_ack_pack);
struct config_pdu_udp reg_ack_send_package_maker (struct config_pdu_udp tcp_package);
void received_tcp_package_treatment(struct pdu_tcp_pack recieved_package);
struct pdu_tcp_pack setup_tcp_package(int element_value, char type);
struct config_pdu_udp client_to_pdu ();
void save_server_info_udp(struct config_pdu_udp server_pck);

/*+++++++++++++++++++++ERROR CONTROLS & BOOLEANS+++++++++++++++++++++*/

bool validate_alive(struct config_pdu_udp recieved_packet);
bool valid_server_udp_comms(struct config_pdu_udp server_pack);
bool valid_server_tcp_comms(struct pdu_tcp_pack server_pack);
void wait_after_send_package(int package_order);
bool data_ack_validate(struct  pdu_tcp_pack recieved_package);
/*+++++++++++++++++++++GLOBAL-FUNCTIONS+++++++++++++++++++++*/
void status_change_message(unsigned char state);
int check_debug_mode(int argc, char *argv[]);
void print_start_status();
void show_status(unsigned char state);
char *get_configname(int argc, char *argv[]);
struct config_info read_config_files(int argc, char *argv[], int debug_mode);
char *read_command();
void command_treatment();
void set_element(char *buffer);
int get_element(char *element);




//Variables globals
int status = DISCONNECTED;
struct socket_pck sock;
struct server_info inf_server;
struct sockaddr_in recieved_pack_addr;
thrd_t command = (thrd_t) NULL;
struct config_info user;
int fake_alive_packets = 0;
bool first_packet_alive = true;

int main(int argc, char *argv[]) {

    int debug_status;
    user = read_config_files(argc, argv, debug_status);

    debug_status = check_debug_mode(argc,argv);
    print_start_status();
    //fase register
    status = NOT_REGISTERED;
    register_client_connection();
    //fase alive
    thrd_create(&command, (thrd_start_t) command_treatment, 0);
    thrd_create(&command,(thrd_start_t) periodic_local_tcp_comms, 0);
    stay_alive(user);
}


/*------------------------------FASE REGISTER---------------------------------------------------*/
void register_client_connection (){

    udp_socket(user);
    int unfinished_register_try = 0;

    while (unfinished_register_try < O && status != REGISTERED){
        printf("Intent de registre: %i \n", unfinished_register_try + 1);
        status_change_message(status);

        for(int i = 0; i < N; i++) {

            struct config_pdu_udp pck_recieved;
            status = WAIT_ACK_REG;
            status_change_message(status);
            send_udp_package(REG_REQ, client_to_pdu());
            pck_recieved = recv_pck_udp(T);

            if (pck_recieved.type == REG_ACK && status == WAIT_ACK_REG){
                save_server_info_udp(pck_recieved);
                pck_recieved = wait_ack_reg_phase(REG_INFO, pck_recieved);
            }
            if (pck_recieved.type == REG_NACK){
                status = NOT_REGISTERED;
                status_change_message(status);
            }
            if (pck_recieved.type == REG_REJ){
                status = NOT_REGISTERED;
                status_change_message(status);
                break;
            }
            if (pck_recieved.type == INFO_ACK && status == WAIT_ACK_INFO && valid_server_udp_comms(pck_recieved)){
                status = REGISTERED;
                status_change_message(status);
                sock.tcp_port = atoi(pck_recieved.dades);
                return;
            }
            if (pck_recieved.type == INFO_NACK && status == WAIT_ACK_INFO && valid_server_udp_comms(pck_recieved)){
                printf("Motiu de falla del registre: %s\n",pck_recieved.dades);
                status = NOT_REGISTERED;
                status_change_message(status);
            }else{
                status = NOT_REGISTERED;
                printf("Paquet invalid de tipus: \n");
                show_status(pck_recieved.type);
            }

            wait_after_send_package(i);
        }
        sleep(U);
        unfinished_register_try++;
    }
    printf("CLIENT NOT REGISTERED \n");
    exit(1);

}


/*--------------------------------------SEND & RECIEVE PACKETS UDP-----------------------------------*/

//UDP
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
    sock.recv_timeout.tv_sec = time_out;


    if (select(sock.udp_socket + 1, &writefds, NULL, NULL, &sock.recv_timeout) > 0) {

        recvfrom(sock.udp_socket, (void *) &reg_server, sizeof(struct config_pdu_udp), 0,(struct sockaddr *)&recieved_pack_addr, (socklen_t *)&recieved_pack_addr);

    }
    return reg_server;

}

//TCP
void send_package_via_tcp_to_server(struct pdu_tcp_pack send_pck, int socket) {
    if (write(socket, &send_pck, sizeof(send_pck)) == -1) {
        printf("Error al enviar paquet via TCP");

    }
}

void receive_tcp_pck_from_server(int time_out, int socket){
    struct  pdu_tcp_pack recieved_pack;

    fd_set writefds;

    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);
    sock.recv_timeout.tv_sec = time_out;


    if (select(socket + 1, &writefds, NULL, NULL, &sock.recv_timeout) > 0){
        recv(socket, &recieved_pack, sizeof(recieved_pack), 0);
        received_tcp_package_treatment(recieved_pack);
    }else{
        status = NOT_REGISTERED;
        printf("Paquet TCP no rebut \n");
        status_change_message(status);

        return;
    }

}



/*------------------ALIVE PHASE----------------------------*/

void stay_alive(struct  config_info client_info){
    periodic_communication(client_info);
}

void periodic_communication(struct config_info client){

    struct config_pdu_udp alive_packet;

    alive_packet = client_to_pdu();
    strcpy(alive_packet.id_comm, inf_server.id_comm);
    int i = 0;


        while(1){
            struct config_pdu_udp recieved_packet_alive;
            struct pdu_tcp_pack recieved_package;
            strcpy(alive_packet.id_comm, inf_server.id_comm);
            send_udp_package(ALIVE, alive_packet);
            recieved_packet_alive = recv_pck_udp(R);

            if(validate_alive(recieved_packet_alive) && first_packet_alive){
                first_packet_alive = false;
                status = SEND_ALIVE;
                status_change_message(status);

            }else if(valid_server_udp_comms(recieved_packet_alive) && recieved_packet_alive.type == ALIVE_REJ){
                status = NOT_REGISTERED;

            }else if(!validate_alive(recieved_packet_alive)){
                fake_alive_packets++;
                if(first_packet_alive){
                    status = NOT_REGISTERED;

                }else if(fake_alive_packets == S){
                    status = NOT_REGISTERED;

                }

            }
            if(status == NOT_REGISTERED){
                first_packet_alive = true;
                fake_alive_packets = 0;
                status_change_message(status);
                register_client_connection();
            }
            sleep(V);
        }

}

void periodic_local_tcp_comms(){
    int i = 1;
    while (i > 0){
        if(status == SEND_ALIVE){
            tcp_socket_recv();
            i = 0;
        }
    }


}



/*-----------------------------------------------------------SOCKETS UDP & TCP----------------------------------*/
void udp_socket(){
    struct sockaddr_in client_addr;
    struct hostent *ent;

    sock.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.udp_socket < 0){
        printf("Error al crear Socket %i", sock.udp_socket);
        exit(-1);
    }

    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(0);
    client_addr.sin_family = AF_INET;

    if(bind(sock.udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0 ){
        printf("connexio dolenta");
        exit(-1);
    }


    ent = gethostbyname((const char*) user.server);

    if(!ent)
    {
        printf("Error! No trobat: %s \n",user.server);
        exit(-1);
    }

    memset(&sock.udp_addr, 0, sizeof(struct sockaddr_in));
    sock.udp_addr.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    sock.udp_addr.sin_port = htons(atoi((const char *)&user.server_udp ));
    sock.udp_addr.sin_family = AF_INET;


}

void tcp_socket(){
    struct hostent *ent;
    ent = gethostbyname((char *)&user.server);

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

void tcp_socket_recv(){
    int ssock_cli, connection;
    struct sockaddr_in client_tcp, server_addr;

    sock.tcp_socket_recv = socket(AF_INET, SOCK_STREAM, 0);
    if(sock.tcp_socket_recv < 0){
        printf("Error al crear Socket tcp \n");
        exit(1);
    }

    memset(&client_tcp, 0, sizeof(struct sockaddr_in));
    client_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
    client_tcp.sin_port = htons(atoi((const char *)user.local_TCP));
    client_tcp.sin_family = AF_INET;

    if(bind(sock.tcp_socket_recv, (struct sockaddr *)&client_tcp, sizeof(client_tcp)) < 0 ){
        printf("connexio dolenta");
        exit(-1);
    }
    printf("Obert port %s TCP\n",user.local_TCP);

    if(listen(sock.tcp_socket_recv, 5) != 0){
        printf("Error in listening \n");
        exit(1);
    }

    while (1){
        ssock_cli = sizeof(server_addr);
        connection = accept(sock.tcp_socket_recv, (struct sockaddr*)&server_addr, &ssock_cli);

        if(connection > 0){
            struct pdu_tcp_pack recv_pck;
            receive_tcp_pck_from_server(3, connection);
            printf("entra \n");
        }

    }

}


/*--------------------------------------------PACKET TREATMENT-------------------*/
struct config_pdu_udp client_to_pdu (){

    struct config_pdu_udp reg_pack;

    strcpy((char *) reg_pack.id_transm,(const char *) user.id);
    strcpy((char *) reg_pack.id_comm,"0000000000");
    strcpy(reg_pack.dades, "");

    return reg_pack;
}

struct config_pdu_udp reg_ack_send_package_maker(struct config_pdu_udp tcp_package){
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

struct pdu_tcp_pack setup_tcp_package(int element_value, char type){

    struct  pdu_tcp_pack send_package;

    char hora[9];
    char any[5], mes[3], dia[3];
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);

    strftime(hora, sizeof(hora), "%X", tm_struct);
    strftime(any, sizeof(any), "%Y", tm_struct);
    strftime(mes, sizeof(mes), "%m", tm_struct);
    strftime(dia, sizeof(dia), "%d", tm_struct);

    send_package.type = type;
    strcpy(send_package.id_transm, (const char *)&user.id);
    strcpy(send_package.id_comm, inf_server.id_comm);
    strcpy(send_package.element, (const char *)user.elements[element_value]);
    strcpy(send_package.valor, (const char *)&user.valor_elements[element_value]);

    strcpy((char *)&send_package.info[0], any);
    strcpy((char *)&send_package.info[0 + 4], "-");
    strcpy((char *)&send_package.info[0 + 4 + 1], mes);
    strcpy((char *)&send_package.info[0 + 4 + 1 + 2], "-");
    strcpy((char *)&send_package.info[0 + 4 + 1 + 2 + 1], dia);
    strcpy((char *)&send_package.info[0 + 4 + 1 + 2 + 1 + 2], ";");
    strcpy((char *)&send_package.info[0 + 4 + 1 + 2 + 1 + 2 + 1], hora);


    return send_package;
}

void received_tcp_package_treatment(struct pdu_tcp_pack recieved_package){


    if(data_ack_validate(recieved_package)){
        printf("Element: %s actualitzat \n",recieved_package.element);
        return;
    }else if(valid_server_tcp_comms(recieved_package) && recieved_package.type == DATA_NACK){
        printf("Element: %s no s'ha actualitzat \n",recieved_package.element);
        return;
    }else if(valid_server_tcp_comms(recieved_package) && recieved_package.type == SET_DATA){
        printf("rebut set data \n");
        return;
    }else if(valid_server_tcp_comms(recieved_package) && recieved_package.type == GET_DATA){
        printf("rebut get data \n");
        return;
    }else{
        printf("Paquet TCP incorrecte, tornant a l'estat:");
        status = NOT_REGISTERED;
        show_status(status);
        return;
    }

}

struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct  config_pdu_udp reg_ack_pack){

    struct sockaddr_in addr_tcp_port = sock.udp_addr;
    unsigned char server_port[6];
    int bytes_send = 0;
    struct config_pdu_udp info_ack_packet;

    reg_ack_pack.type = REG_INFO;

    //Creem copia de la adreça i canviem port
    strcpy((char *)server_port, reg_ack_pack.dades);
    reg_ack_pack = reg_ack_send_package_maker(reg_ack_pack);
    addr_tcp_port.sin_port = htons(atoi((const char*)&server_port));


    sendto(sock.udp_socket, (void *)&reg_ack_pack, sizeof(reg_ack_pack), 0, (const struct sockaddr*)&addr_tcp_port, sizeof(addr_tcp_port));

    status = WAIT_ACK_INFO;
    status_change_message(status);
    info_ack_packet = recv_pck_udp(2*T);

    return info_ack_packet;
}

/*--------------------------------------------ERROR CONTROLS & BOOLEANS-------------------*/

bool valid_server_udp_comms(struct config_pdu_udp server_pack){

    if(strcmp(inf_server.id_comm, server_pack.id_comm) == 0 && strcmp(inf_server.id_transm, server_pack.id_transm) == 0 && sock.udp_addr.sin_addr.s_addr == recieved_pack_addr.sin_addr.s_addr){
        return true;
    }else{
        return false;
    }
}

bool valid_server_tcp_comms(struct pdu_tcp_pack server_pack){

    if(strcmp(inf_server.id_comm, server_pack.id_comm) == 0 && strcmp(inf_server.id_transm, server_pack.id_transm) == 0 && sock.udp_addr.sin_addr.s_addr == recieved_pack_addr.sin_addr.s_addr){
        return true;
    }else{
        return false;
    }
}

void save_server_info_udp(struct config_pdu_udp server_pck){

    strcpy(inf_server.id_transm, server_pck.id_transm);
    strcpy(inf_server.id_comm, server_pck.id_comm);
    strcpy((char *)&inf_server.port, server_pck.dades);

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

bool validate_alive(struct config_pdu_udp recieved_packet){

    if (valid_server_udp_comms(recieved_packet) && strcmp(recieved_packet.dades, (const char*)&user.id) == 0 && recieved_packet.type == ALIVE){
        return  true;
    } else{
        return false;
    }
}

bool data_ack_validate(struct  pdu_tcp_pack recieved_package){

    return (valid_server_tcp_comms(recieved_package) &&
            recieved_package.type == DATA_ACK &&
            strcmp(recieved_package.info, (const char *)&user.id) == 0 &&
            get_element(recieved_package.element) < 6);

}
/*------------------GENERAL FUNCTIONS----------------------------*/

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

void print_start_status() {
    printf("********************* DADES DISPOSITIU ***********************\n");
    printf("Identificador: %s\n", user.id);
    show_status(status);
    printf(" Param      valor\n");
    printf(" -----      --------\n");
    int i = 0;

   while(strcmp((char *)user.elements[i],"NULL") != 0){
           printf("%s       %s\n", user.elements[i],user.valor_elements[i]);
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
        case DATA_ACK:
            printf("DATA_ACK\n");
            break;
        case DATA_NACK:
            printf("DATA_NACK\n");
            break;
        case DATA_REJ:
            printf("DATA_REJ\n");
            break;
    }
}

void status_change_message(unsigned char state){
    int h, m, s;
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);
    h = tm_struct -> tm_hour;
    m = tm_struct -> tm_min;
    s = tm_struct -> tm_sec;

    printf("%i:%i:%i MSG: Dispositiu passa a l'estat: ", h, m, s);
    show_status(state);

}

//Command-phase
void command_treatment(){

    while(1){
        char *command = read_command();
        if(strstr(command, "status") == command && status == SEND_ALIVE){
            print_start_status(user);
        }else if(strstr(command, "set") == command && status == SEND_ALIVE){
            set_element(command);
        }else if(strstr(command, "send") == command && status == SEND_ALIVE){
            char *element;
            char  *cpy_token;

            //Obtain element
            cpy_token = strtok(command, " ");
            cpy_token = strtok(NULL, " ");
            if(cpy_token == NULL){
                printf("Us comanda: send <identificador_element>\n");
            }else{
                element = cpy_token;
                int i = get_element(element);

                if(i < 6){
                    struct pdu_tcp_pack send_package;
                    struct pdu_tcp_pack recieved_package;

                    tcp_socket();
                    send_package = setup_tcp_package(i, SEND_DATA);
                    send_package_via_tcp_to_server(send_package, sock.tcp_socket);
                    receive_tcp_pck_from_server(M, sock.tcp_socket);

                    close(sock.tcp_socket);
                }else{
                    printf("Element: %s no existeix \n", element);
                }
            }
        }else{
            printf("Comanda no reconeguda \n");
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

void set_element(char *buffer){
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


    for(int i = 0; i < 6;i++){

        if(strcmp((const char *)&user.elements[i], element) == 0){
            strcpy((char *)&user.valor_elements[i], element_value);
            return;
        }

    }
    printf("Element no trobat \n");
}

int get_element(char *element){

    for(int i = 0; i < 6;i++){

        if(strcmp((const char *)&user.elements[i], element) == 0){
            return i;
        }

    }
    return 10;

}










