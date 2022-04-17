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
#define S 3
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
    int connection_fd;
    struct sockaddr_in udp_addr;
    struct sockaddr_in tcp_addr;
    struct timeval recv_timeout;
};

struct pdu_tcp_pack {
    unsigned char type;
    char id_transm[11];
    char id_comm[11];
    char element[8];
    char valor[16];
    char info[80];
};

/*+++++++++++++++++++++REGISTER-FUNCTIONS+++++++++++++++++++++*/
void register_client_connection();

/*+++++++++++++++++++++SOCKETS TCP & UDP+++++++++++++++++++++*/

void tcp_socket();

void tcp_socket_recv();

void udp_socket();

/*+++++++++++++++++++++ALIVE-STATE-FUNCTIONS+++++++++++++++++++++*/

void periodic_communication();

void periodic_local_tcp_comms();

/*+++++++++++++++++++++SEND & RECEIVED UDP/TCP+++++++++++++++++++++*/

void send_udp_package(unsigned char type, struct config_pdu_udp packet_to_send);

struct config_pdu_udp recv_pck_udp(int time_out);

void send_package_via_tcp_to_server(struct pdu_tcp_pack send_pck, int socket);

void receive_tcp_pck_from_server(int time_out, int socket);

/*+++++++++++++++++++++PACKET CONTROL+++++++++++++++++++++*/

struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct config_pdu_udp info_ack_packet);

struct config_pdu_udp reg_info_send_package_maker(struct config_pdu_udp tcp_package);

void received_tcp_package_treatment(struct pdu_tcp_pack recieved_package);

struct pdu_tcp_pack setup_tcp_package(int element_value, int type);

struct config_pdu_udp client_to_pdu();

void save_server_info_udp(struct config_pdu_udp server_pck);

/*+++++++++++++++++++++ERROR CONTROLS & BOOLEANS+++++++++++++++++++++*/

bool validate_alive(struct config_pdu_udp recieved_packet);

bool valid_server_udp_comms(struct config_pdu_udp server_pack);

bool valid_server_tcp_comms(struct pdu_tcp_pack server_pack);

void wait_after_send_package(int package_order);

bool data_ack_validate(struct pdu_tcp_pack recieved_package);

/*+++++++++++++++++++++GLOBAL-FUNCTIONS+++++++++++++++++++++*/
void status_change_message(unsigned char state);

int check_debug_mode(int argc, char *argv[]);

void print_status();

char *str_status_or_type(unsigned char state);

char *get_configname(int argc, char *argv[]);

struct config_info read_config_files(int argc, char *argv[], int debug_mode);

char *read_command();

void command_treatment();

void set_element(char *buffer);

int get_element(char *element);


//Variables globals
int status = DISCONNECTED;
int no_consecutive_alive_packets = 0;
int debug_mode = 0;
int unfinished_register_try = 0;

struct socket_pck sock;
struct server_info inf_server;
struct sockaddr_in recieved_pack_addr;
struct config_info user;

thrd_t command = (thrd_t) NULL;
bool first_packet_alive = true;

int main(int argc, char *argv[]) {

    user = read_config_files(argc, argv, debug_mode);
    debug_mode = check_debug_mode(argc, argv);
    print_status();

    //fase register inicia amb el client NOT_REGISTERED
    status = NOT_REGISTERED;
    register_client_connection();

    //Una vegada pasa l'estat register, creem dos fils, un conte les comandes disponibles per a l'estat alive,
    // i l'altre es la funcio de comunicació periodica amb el servidor.

    thrd_create(&command, (thrd_start_t) command_treatment, 0);
    thrd_create(&command, (thrd_start_t) periodic_local_tcp_comms, 0);
    periodic_communication();
}


/*------------------------------FASE REGISTER---------------------------------------------------*/
void register_client_connection() {
    //Iniciem el socket UDP per el qual realitzarem la fase de Registre i de comunicació periodica
    udp_socket(user);

    while (unfinished_register_try < O && status != REGISTERED) {
        //Sol tindrem 3 intents de registre, incloent els que iniciem en cas de una falla de Alives consecutius
        printf("Intent de registre: %i \n", unfinished_register_try + 1);
        status = NOT_REGISTERED;
        status_change_message(status);

        for (int i = 0; i < N; i++) {
            //Funcio de tractament de paquets UDP(Sol fase registre)
            struct config_pdu_udp pck_recieved;

            if (status != WAIT_ACK_REG) {
                status = WAIT_ACK_REG; //Per evitar repeticions de missatges, si el estat es el mateix no es tornara a imprimir
                status_change_message(status);
            }
            //Enviem UDP base REG_REQ
            send_udp_package(REG_REQ, client_to_pdu());
            pck_recieved = recv_pck_udp(T);

            if (pck_recieved.type == REG_ACK && status == WAIT_ACK_REG) {
                //En arribar a tindre un REG_ACK, guardarem l'info del server en el que estem completant el registre i iniciem la fase corresponent a aquest paquet
                save_server_info_udp(pck_recieved);//Funcio que gestiona la resposta al REG_ACK (Retorna paquet INFO_---)
                pck_recieved = wait_ack_reg_phase(REG_INFO, pck_recieved);
            }
            if (pck_recieved.type == REG_NACK) {
                status = NOT_REGISTERED;
                status_change_message(status);
            }
            if (pck_recieved.type == REG_REJ) {
                status = NOT_REGISTERED;
                status_change_message(status);
                break;
            }
            if (pck_recieved.type == INFO_ACK && status == WAIT_ACK_INFO && valid_server_udp_comms(pck_recieved)) {
                //Completem registre i guardem el port tcp del servidor rebut al INFO_ACK
                status = REGISTERED;
                status_change_message(status);
                sock.tcp_port = atoi(pck_recieved.dades);
                return;
            }
            if (pck_recieved.type == INFO_NACK && status == WAIT_ACK_INFO && valid_server_udp_comms(pck_recieved)) {
                printf("Motiu de falla del registre: %s\n", pck_recieved.dades);
                status = NOT_REGISTERED;
                status_change_message(status);
            } else if (status == WAIT_ACK_INFO) {
                printf("INFO_ACK no rebut \n");
                status = NOT_REGISTERED;
                status_change_message(NOT_REGISTERED);
                break;
            }
            wait_after_send_package(i);
        }
        sleep(U);
        unfinished_register_try++;
        //Fins aqui es completa un intent de registre
    }
    //Si hem superat el numero d'intents avisem per terminal i tanquem programa
    printf("CLIENT NOT REGISTERED \n");
    exit(1);
}


/*--------------------------------------SEND & RECIEVE PACKETS UDP-----------------------------------*/
//Aqui tenim totes les funcions d'enviar i rebre ordenades en UDP_TCP

//UDP
void send_udp_package(unsigned char type, struct config_pdu_udp packet_to_send) {
    struct config_pdu_udp reg_str = packet_to_send;
    reg_str.type = type;

    sendto(sock.udp_socket, (void *) &reg_str, sizeof(reg_str), 0,
           (const struct sockaddr *) &sock.udp_addr, sizeof(sock.udp_addr));

    if (debug_mode == 1) {
        printf("UDP Packet Send -> Type : %s Id_trans: %s Id_comm: %s Dades: %s \n",
               str_status_or_type(reg_str.type), reg_str.id_transm, reg_str.id_comm, reg_str.dades);
    }

}//Funcio d'enviar per UDP

struct config_pdu_udp recv_pck_udp(int time_out) {
    struct config_pdu_udp reg_server;
    int recv_addr_size = sizeof(recieved_pack_addr);

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock.udp_socket, &writefds);
    sock.recv_timeout.tv_sec = time_out;

    if (select(sock.udp_socket + 1, &writefds, NULL, NULL, &sock.recv_timeout) > 0) {

        recvfrom(sock.udp_socket, &reg_server, sizeof(struct config_pdu_udp), 0,
                (struct sockaddr *) &recieved_pack_addr, (socklen_t * ) & recv_addr_size);

        if (debug_mode == 1) {
            printf("UDP Packet Recv ->Type: %s Id_trans: %s Id_comm: %s Dades: %s \n",
                   str_status_or_type(reg_server.type),
                   reg_server.id_transm, reg_server.id_comm, reg_server.dades);
        }
    }
    return reg_server;
}//Funcio de rebre per UDP

//TCP


void send_package_via_tcp_to_server(struct pdu_tcp_pack send_pck, int socket) {
    //Funcio d'enviar per TCP
    if (write(socket, &send_pck, sizeof(send_pck)) == -1) {
        printf("Error al enviar paquet via TCP \n");
    }
    if (debug_mode == 1) {
        printf("TCP Packet Send -> Type : %s Id_trans: %s Id_comm: %s Element: %s Valor: %s Info: %s  \n",
               str_status_or_type(send_pck.type), send_pck.id_transm, send_pck.id_comm, send_pck.element,
               send_pck.valor, send_pck.info);
    }
}

void receive_tcp_pck_from_server(int time_out, int socket) {
    //Funcio de rebre paquets per TCP
    struct pdu_tcp_pack recieved_pack;
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);
    sock.recv_timeout.tv_sec = time_out;

    if (select(socket + 1, &writefds, NULL, NULL, &sock.recv_timeout) > 0) {
        recv(socket, &recieved_pack, sizeof(recieved_pack), 0);
        if (debug_mode == 1) {
            printf("TCP Packet Recieved -> Type : %s Id_trans: %s Id_comm: %s Element: %s Valor: %s Info: %s  \n",
                   str_status_or_type(recieved_pack.type), recieved_pack.id_transm, recieved_pack.id_comm,
                   recieved_pack.element, recieved_pack.valor, recieved_pack.info);
        }
        received_tcp_package_treatment(recieved_pack);
    } else {
        status = NOT_REGISTERED;
        printf("Paquet TCP no rebut \n");
        status_change_message(status);
        return;
    }
}

/*------------------ALIVE PHASE----------------------------*/



void periodic_communication() {
    /*Funcio que controla la comunicacio periodica amb el servidor.
     * Comptara fins a tres ALIVES consecutius no rebuts,si aixo pasa,
     * es tornara a la fase de registre considerant un nou intent de
     * regristre que es sumara a la variable publica unfinished_register_try.
     * En cas de rebre un ALIVE es reiniciara el contador no_consecutive_alive_packets a 0
     */

    struct config_pdu_udp alive_packet;
    alive_packet = client_to_pdu();
    strcpy(alive_packet.id_comm, inf_server.id_comm);

    while (1) {
        struct config_pdu_udp recieved_packet_alive;

        strcpy(alive_packet.id_comm, inf_server.id_comm);
        send_udp_package(ALIVE, alive_packet);
        recieved_packet_alive = recv_pck_udp(R);

        if (validate_alive(recieved_packet_alive) && first_packet_alive) {
            first_packet_alive = false;
            status = SEND_ALIVE;
            status_change_message(status);
        } else if (valid_server_udp_comms(recieved_packet_alive) && recieved_packet_alive.type == ALIVE_REJ) {
            status = NOT_REGISTERED;
        } else if (validate_alive(recieved_packet_alive)) {
            no_consecutive_alive_packets = 0;
        }else if (!validate_alive(recieved_packet_alive)) {
            if(debug_mode == 1){
                printf("ALIVE Invalid\n");
            }
            no_consecutive_alive_packets++;
            if (first_packet_alive) {
                status = NOT_REGISTERED;
            } else if (no_consecutive_alive_packets == S) {
                status = NOT_REGISTERED;
            }
        }
        if (status == NOT_REGISTERED) {
            first_packet_alive = true;
            no_consecutive_alive_packets = 0;
            status_change_message(status);
            unfinished_register_try++;
            register_client_connection();
        }
        sleep(V);
    }
}

void periodic_local_tcp_comms() {
    //Funcio que espera a estar amb estat SEND_ALIVE per obrir comunicacio TCP
    int i = 1;
    while (i > 0) {
        if (status == SEND_ALIVE) {
            tcp_socket_recv();
            i = 0;
        }
    }
}


/*-----------------------------------------------------------SOCKETS UDP & TCP----------------------------------*/
void udp_socket() {
    //Funcio que obra Socket UDP amb el port udp del archiu de configuracions

    struct sockaddr_in client_addr;
    struct hostent *ent;

    sock.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.udp_socket < 0) {
        printf("Error al crear Socket %i", sock.udp_socket);
        exit(-1);
    }

    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(0);
    client_addr.sin_family = AF_INET;

    if (bind(sock.udp_socket, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
        printf("connexio dolenta");
        exit(-1);
    }


    ent = gethostbyname((const char *) user.server);

    if (!ent) {
        printf("Error! No trobat: %s \n", user.server);
        exit(-1);
    }

    memset(&sock.udp_addr, 0, sizeof(struct sockaddr_in));
    sock.udp_addr.sin_addr.s_addr = (((struct in_addr *) ent->h_addr_list[0])->s_addr);
    sock.udp_addr.sin_port = htons(atoi((const char *) &user.server_udp));
    sock.udp_addr.sin_family = AF_INET;


}

void tcp_socket() {
    //Funcio que obra Socket TCP linkat amb el port del server
    struct hostent *ent;
    ent = gethostbyname((char *) &user.server);

    if (!ent) {
        printf("Server no trobat creant socket TCP\n");
        exit(1);
    }

    sock.tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sock.tcp_socket < 0) {
        printf("Error al crear Socket tcp \n");
        exit(1);
    }

    memset(&sock.tcp_addr, 0, sizeof(struct sockaddr_in));
    sock.tcp_addr.sin_family = AF_INET;
    sock.tcp_addr.sin_addr.s_addr = (((struct in_addr *) ent->h_addr_list[0])->s_addr);
    sock.tcp_addr.sin_port = htons(sock.tcp_port);

    if (connect(sock.tcp_socket, (struct sockaddr *) &sock.tcp_addr, sizeof(sock.tcp_addr)) < 0) {
        printf("Error al connectar amb server via TCP\n");
        exit(1);
    }

}

void tcp_socket_recv() {
    //Funcio que obra Socket tcp linkat amb el port del client
    int ssock_cli;
    struct sockaddr_in client_tcp, server_addr;

    sock.tcp_socket_recv = socket(AF_INET, SOCK_STREAM, 0);
    if (sock.tcp_socket_recv < 0) {
        printf("Error al crear Socket tcp \n");
        exit(1);
    }

    memset(&client_tcp, 0, sizeof(struct sockaddr_in));
    client_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
    client_tcp.sin_port = htons(atoi((const char *) user.local_TCP));
    client_tcp.sin_family = AF_INET;

    if (bind(sock.tcp_socket_recv, (struct sockaddr *) &client_tcp, sizeof(client_tcp)) < 0) {
        printf("Port TCP ocupat, connexio dolenta...");
        exit(-1);
    }
    printf("Obert port %s TCP\n", user.local_TCP);

    if (listen(sock.tcp_socket_recv, 5) != 0) {
        printf("Error in listening \n");
        exit(1);
    }

    while (1) { //Bucle que accepta connexions del server i rep paquets d'aquest
        ssock_cli = sizeof(server_addr);
        sock.connection_fd = accept(sock.tcp_socket_recv, (struct sockaddr *) &server_addr, (socklen_t * ) & ssock_cli);

        if (sock.connection_fd > 0) {
            receive_tcp_pck_from_server(3, sock.connection_fd);
        }
    }
}


/*--------------------------------------------PACKET TREATMENT-------------------*/
//Funcions que creen paquets o els tracten

struct config_pdu_udp client_to_pdu() {
    //Crea PDU a partir de la info del client

    struct config_pdu_udp reg_pack;

    strcpy((char *) reg_pack.id_transm, (const char *) user.id);
    strcpy((char *) reg_pack.id_comm, "0000000000");
    strcpy(reg_pack.dades, "");

    return reg_pack;
}

struct config_pdu_udp reg_info_send_package_maker(struct config_pdu_udp tcp_package) {
    //Prepara el paquet a enviar en la fase REG_ACK
    int i = 0;
    int pos = 5;

    strcpy((char *) &tcp_package.id_transm, (const char *) &user.id);
    strcpy((char *) &tcp_package.dades[0], (const char *) &user.local_TCP);
    strcpy((char *) &tcp_package.dades[4], (const char *) ",");

    while (strcmp((char *) &user.elements[i], "NULL") != 0 && i < 5) {
        strcpy((char *) &tcp_package.dades[pos], (const char *) &user.elements[i]);

        if (strcmp((char *) &user.elements[i + 1], "NULL") != 0 && i < 4) {
            strcpy((char *) &tcp_package.dades[pos + 7], (const char *) ";");
        }
        pos += 8;
        i++;
    }
    return tcp_package;
}

struct pdu_tcp_pack setup_tcp_package(int element_value, int type) {
    //Crea paquets TCP amb la PDU corresponent
    struct pdu_tcp_pack send_package;

    char hora[9];
    char any[5], mes[3], dia[3];
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);

    strftime(hora, sizeof(hora), "%X", tm_struct);
    strftime(any, sizeof(any), "%Y", tm_struct);
    strftime(mes, sizeof(mes), "%m", tm_struct);
    strftime(dia, sizeof(dia), "%d", tm_struct);

    send_package.type = type;
    strcpy(send_package.id_transm, (const char *) &user.id);
    strcpy(send_package.id_comm, inf_server.id_comm);
    strcpy(send_package.element, (const char *) user.elements[element_value]);
    strcpy(send_package.valor, (const char *) &user.valor_elements[element_value]);

    strcpy((char *) &send_package.info[0], any);
    strcpy((char *) &send_package.info[0 + 4], "-");
    strcpy((char *) &send_package.info[0 + 4 + 1], mes);
    strcpy((char *) &send_package.info[0 + 4 + 1 + 2], "-");
    strcpy((char *) &send_package.info[0 + 4 + 1 + 2 + 1], dia);
    strcpy((char *) &send_package.info[0 + 4 + 1 + 2 + 1 + 2], ";");
    strcpy((char *) &send_package.info[0 + 4 + 1 + 2 + 1 + 2 + 1], hora);

    return send_package;
}

void received_tcp_package_treatment(struct pdu_tcp_pack recieved_package) {
    //Tracta els paquets rebuts via tcp
    struct pdu_tcp_pack send_package;
    int n_element = get_element(recieved_package.element);

    if (data_ack_validate(recieved_package)) {
        printf("Element: %s actualitzat al servidor \n", recieved_package.element);
        return;
    }

    if (valid_server_tcp_comms(recieved_package) && recieved_package.type == DATA_NACK) {
        printf("Element: %s no s'ha actualitzat al servidor \n", recieved_package.element);
        return;
    }

    if (valid_server_tcp_comms(recieved_package) && recieved_package.type == SET_DATA) {
        //Tractament de SET_DATA
        if (n_element < 6 && user.elements[n_element][6] == 'I') {
            strcpy((char *) &user.valor_elements[n_element], recieved_package.valor);
            printf("Element : %s actualitzat, nou valor : %s\n", user.elements[n_element], recieved_package.valor);
            send_package = setup_tcp_package(n_element, DATA_ACK);
            send_package_via_tcp_to_server(send_package, sock.connection_fd);
        } else {
            send_package = setup_tcp_package(n_element, DATA_NACK);
            strcpy(send_package.valor, recieved_package.valor);
            strcpy(send_package.info, "Element invalid o no es d'entrada\n");
            send_package_via_tcp_to_server(send_package, sock.connection_fd);
        }
        return;
    }

    if (valid_server_tcp_comms(recieved_package) && recieved_package.type == GET_DATA) {
        //Tractament de GET_DATA
        if (n_element < 6) {
            send_package = setup_tcp_package(n_element, DATA_ACK);
            printf("Element : %s enviat al servidor\n", user.elements[n_element]);
            send_package_via_tcp_to_server(send_package, sock.connection_fd);
        }else{
            send_package = setup_tcp_package(n_element, DATA_NACK);
            strcpy(send_package.valor, recieved_package.valor);
            strcpy(send_package.info, "Element invalid\n");
            send_package_via_tcp_to_server(send_package, sock.connection_fd);
        }
        return;
    } else {
        //En cas de tindre dades incorrectes al GET_DATA o SET_DATA es tracten per igual i enviem DATA_REJ
        if (recieved_package.type == GET_DATA || recieved_package.type == SET_DATA) {
            send_package = setup_tcp_package(n_element, DATA_REJ);
            strcpy(send_package.valor, recieved_package.valor);
            strcpy(send_package.info, "Error en les dades d'identificacio");
            send_package_via_tcp_to_server(send_package, sock.connection_fd);
        }

        printf("Paquet TCP incorrecte, tornant a l'estat: %s \n", str_status_or_type(NOT_REGISTERED));
        status = NOT_REGISTERED;
        return;
    }

}

struct config_pdu_udp wait_ack_reg_phase(unsigned char type, struct config_pdu_udp reg_ack_pack) {
    //Funcio que tracta el REG_ACK rebut

    struct sockaddr_in addr_tcp_port = sock.udp_addr;
    unsigned char server_port[6];
    struct config_pdu_udp reg_info_packet = reg_ack_pack;

    //Creem copia de la adreça amb el port rebut i canviem port per on enviarem REG_INFO
    reg_info_packet.type = REG_INFO;
    printf("Port udp nou %s\n", reg_info_packet.dades);
    strcpy((char *) server_port, reg_info_packet.dades);
    reg_info_packet = reg_info_send_package_maker(reg_info_packet);
    addr_tcp_port.sin_port = htons(atoi((const char *) &server_port));

    //Enviem paquet via UDP per port especial rebut del servidor
    sendto(sock.udp_socket, (void *) &reg_info_packet, sizeof(reg_info_packet), 0, (const struct sockaddr *) &addr_tcp_port,
           sizeof(addr_tcp_port));
    if (debug_mode == 1) {
        printf("UDP Packet Send ->Type: %s Id_trans: %s Id_comm: %s Dades: %s \n",
               str_status_or_type(reg_info_packet.type), reg_info_packet.id_transm, reg_info_packet.id_comm, reg_info_packet.dades);
    }

    status = WAIT_ACK_INFO;
    status_change_message(status);
    reg_info_packet = recv_pck_udp(2 * T);

    return reg_info_packet;
}

/*--------------------------------------------ERROR CONTROLS & BOOLEANS----------------------------------------*/
//Totes les funcions en aquest apartat tenen com a funcio simplificare el codi i no
// trobar 4 linies de codig dintre dels condicionals

bool valid_server_udp_comms(struct config_pdu_udp server_pack) {
    if (strcmp(inf_server.id_comm, server_pack.id_comm) == 0 &&
        strcmp(inf_server.id_transm, server_pack.id_transm) == 0 &&
        sock.udp_addr.sin_addr.s_addr == recieved_pack_addr.sin_addr.s_addr) {
        return true;
    } else {
        return false;
    }
}

bool valid_server_tcp_comms(struct pdu_tcp_pack server_pack) {
    return strcmp(inf_server.id_comm, server_pack.id_comm) == 0 &&
           strcmp(inf_server.id_transm, server_pack.id_transm) == 0 &&
           sock.udp_addr.sin_addr.s_addr == recieved_pack_addr.sin_addr.s_addr &&
           strcmp(server_pack.info, (const char *) &user.id) == 0;

}

void save_server_info_udp(struct config_pdu_udp server_pck) {

    strcpy(inf_server.id_transm, server_pck.id_transm);
    strcpy(inf_server.id_comm, server_pck.id_comm);
    strcpy((char *) &inf_server.port, server_pck.dades);

}

/*Tambe incloeixo com a control d'errors la funcio que calcula el interval
de temps al fallar els sends en la fase de registre */

void wait_after_send_package(int package_order) {
    //Funcio que calcula interval de temps a dormir al enviar x paquets
    if (package_order > P) {
        if ((T + (T * (package_order - 2))) < T * Q) {
            sleep(T + (T * (package_order - 2)));
        } else {
            sleep(Q * T);
        }
    } else {
        sleep(T);
    }
}

bool validate_alive(struct config_pdu_udp recieved_packet) {

    if (valid_server_udp_comms(recieved_packet) && strcmp(recieved_packet.dades, (const char *) &user.id) == 0 &&
        recieved_packet.type == ALIVE) {
        return true;
    } else {
        return false;
    }
}

bool data_ack_validate(struct pdu_tcp_pack recieved_package) {

    return (valid_server_tcp_comms(recieved_package) &&
            recieved_package.type == DATA_ACK &&
            strcmp(recieved_package.info, (const char *) &user.id) == 0 &&
            get_element(recieved_package.element) < 6);

}

/*------------------GENERAL FUNCTIONS----------------------------*/

struct config_info read_config_files(int argc, char *argv[], int debug_mode) { //Funcio que llegueix la configuracio del client
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
    while (cpy_token != NULL) {
        strcpy((char *) client.elements[i], cpy_token);
        cpy_token = strtok(NULL, ";");
        i++;
    }
    for (int x = i; x < 6; x++) {
        strcpy((char *) client.elements[i], "NULL");
    }

    //local_tcp
    fgets(info, MAX_CHAR, file);
    info[strlen(info) - 1] = '\0';
    cpy_token = strtok(info, " ");
    cpy_token = strtok(NULL, "= ");
    strcpy((char *) client.local_TCP, cpy_token);

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


    if (debug_mode == 1) {
        printf("Configuració del client guardada!!\n Id: %s \n Elements: %s ...\n Local_TCP: %s \n Server: %s \n Server_UDP: %s \n",
               client.id, client.elements[1], client.local_TCP, client.server, client.server_udp);
    }
    return client;
}

int check_debug_mode(int argc, char *argv[]) {
    //Detecta si hem activat el mode debug
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            printf("DEBUG_MODE = ON \n");
            return 1;
        }
    }
    printf("DEBUG_MODE = OFF\n");
    return 0;
}

char *get_configname(int argc, char *argv[]) {
    //Obtenim el nom del client.cfg a llegir
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (argv[i + 1] != NULL) {
                return argv[i + 1];
            }
            printf("Client no valid, lleguirem el client per defecte..\n");
            break;
        }
    }
    return "client.cfg";
}

void print_status() {
    //Imprimeix a l'inici del programa i quan emprem la comanda status, el conjunt d'elements del client
    printf("********************* DADES DISPOSITIU ***********************\n");
    printf("Identificador: %s\n", user.id);
    printf("Estat: %s \n", str_status_or_type(status));
    printf(" Param      valor\n");
    printf(" -----      --------\n");

    int i = 0;
    while (strcmp((char *) user.elements[i], "NULL") != 0) {
        printf("%s      %s\n", user.elements[i], user.valor_elements[i]);
        i++;
    }
    printf("***************************************************************\n");
}

char *str_status_or_type(unsigned char state) {
    //Retorna amb string el valor del paquet o estat del client
    switch (state) {
        case 0xf0:
            return "DISCONNECTED";
            break;
        case 0xf1:
            return "NOT_REGISTERED";
            break;
        case 0xf2:
            return "WAIT_ACK_REG";
            break;
        case 0xf3:
            return "WAIT_INFO";
            break;
        case 0xf4:
            return "WAIT_ACK_INFO";
            break;
        case 0xf5:
            return "REGISTERED";
            break;
        case 0xa0:
            return "REG_REQ";
            break;
        case 0xa1:
            return "REG_ACK";
            break;
        case 0xa2:
            return "REG_NACK";
            break;
        case 0xa3:
            return "REG_REJ";
            break;
        case 0xa4:
            return "REG_INFO";
            break;
        case 0xa5:
            return "INFO_ACK";
            break;
        case 0xa6:
            return "INFO_NACK";
            break;
        case 0xa7:
            return "INFO_REJ";
            break;
        case SEND_ALIVE:
            return "SEND_ALIVE";
            break;
        case ALIVE:
            return "ALIVE";
            break;
        case ALIVE_NACK:
            return "ALIVE_NACK";
            break;
        case ALIVE_REJ:
            return "ALIVE_REJ";
            break;
        case SEND_DATA:
            return "SEND_DATA";
            break;
        case GET_DATA:
            return "GET_DATA";
            break;
        case SET_DATA:
            return "SET_DATA";
            break;
        case DATA_ACK:
            return "DATA_ACK";
            break;
        case DATA_NACK:
            return "DATA_NACK";
            break;
        case DATA_REJ:
            return "DATA_REJ";
            break;
    }
    return "UNKNOWN_TYPE";
}

void status_change_message(unsigned char state){//Imprimeix canvis d'estat del client
    int h, m, s;
    time_t time_now = time(NULL);
    struct tm *tm_struct = localtime(&time_now);

    h = tm_struct->tm_hour;
    m = tm_struct->tm_min;
    s = tm_struct->tm_sec;

    printf("%i:%i:%i MSG: Dispositiu passa a l'estat: %s \n", h, m, s, str_status_or_type(state));


}

void command_treatment() {
    //Tracta les comandes que anirem demanant en l'estat SEND_ALIVE del client
    while (1) {
        char *comm = read_command();
        if (strstr(comm, "status") == comm && status == SEND_ALIVE) {
            print_status();
        } else if (strstr(comm, "set") == comm && status == SEND_ALIVE) {
            set_element(comm);
        } else if (strstr(comm, "send") == comm && status == SEND_ALIVE) {
            char *element;
            char *cpy_token;

            //Tractem la comanda sencera del send
            cpy_token = strtok(comm, " ");
            cpy_token = strtok(NULL, " ");
            if (cpy_token == NULL) {
                printf("Us comanda: send <identificador_element>\n");
            } else {
                element = cpy_token;
                int i = get_element(element);
                if (i < 6) {
                    struct pdu_tcp_pack send_package;
                    tcp_socket();
                    send_package = setup_tcp_package(i, SEND_DATA);
                    send_package_via_tcp_to_server(send_package, sock.tcp_socket);
                    receive_tcp_pck_from_server(M, sock.tcp_socket);
                    close(sock.tcp_socket);
                } else {
                    printf("Element: %s no existeix \n", element);
                }
            }
        } else if (strstr(comm, "quit") == comm && status == SEND_ALIVE) {
            //Tanquem canals de comunicacio i matem procesos amb exit(1)
            printf("Tancant client......\n");
            close(sock.tcp_socket);
            close(sock.udp_socket);
            close(sock.connection_fd);
            exit(1);
        } else {
            printf("Comanda no reconeguda \n");
        }
    }
}

char *read_command() {//Funcio que llegueix les comandes emprades a la terminal
    char *comm = malloc(MAX_CHAR);
    char buffer[MAX_CHAR];

    if (fgets(buffer, MAX_CHAR, stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
    strcpy(comm, buffer);
    return comm;
}

void set_element(char *buffer) {
    //Funcio que tracta els SET_DATA o la comanda set del mateix client
    char *cpy_token, *element;
    char element_value[16];

    //Obtain element
    cpy_token = strtok(buffer, " ");
    cpy_token = strtok(NULL, " ");
    if (cpy_token == NULL) {
        printf("Us comanda: set <identificador_element> <nou_valor> \n");
        return;
    }
    element = cpy_token;

    //Obtain value
    cpy_token = strtok(NULL, " ");
    if (cpy_token == NULL) {
        printf("Us comanda: set <identificador_element> <nou_valor> \n");
        return;
    }
    printf("%s\n", cpy_token);
    strncpy(element_value, cpy_token, 16);

    for (int i = 0; i < 6; i++) {
        if (strcmp((const char *) &user.elements[i], element) == 0) {
            strcpy((char *) &user.valor_elements[i], element_value);
            return;
        }
    }
    printf("Element no trobat \n");
}

int get_element(char *element) {
    //Obte l'index de l'element donat com a argument en forma de string

    for (int i = 0; i < 6; i++) {
        if (strcmp((const char *) &user.elements[i], element) == 0) {
            return i;
        }
    }
    return 10; //En cas de no trobarse tornem un valor mes gran que el index possible
}              //aixi a l'hora de tractar errors es facil










