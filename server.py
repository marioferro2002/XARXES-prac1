#!/usr/bin/env python3

import os
import random
import signal
import socket
import struct
import sys
import threading
import time
from datetime import datetime
from datetime import timedelta

Z = 2
W = 3
X = 3
T = 1
M = 3
# Packet_type

REG_REQ = 0xa0
REG_ACK = 0xa1
REG_NACK = 0xa2
REG_REJ = 0xa3
REG_INFO = 0xa4
INFO_ACK = 0xa5
INFO_NACK = 0xa6
INFO_REJ = 0xa7
ALIVE_NACK = 0xb1
ALIVE_REJ = 0xb2
SEND_DATA = 0xc0
DATA_ACK = 0xc1
DATA_NACK = 0xc2
DATA_REJ = 0xc3
SET_DATA = 0xc4
GET_DATA = 0xc5

# Client-server state

DISCONNECTED = 0xf0
NOT_REGISTERED = 0xf1
WAIT_ACK_REG = 0xf2
WAIT_INFO = 0xf3
WAIT_ACK_INFO = 0xf4
REGISTERED = 0xf5
SEND_ALIVE = 0xf6
ALIVE = 0xb0


class Server:
    def __init__(self):
        self.id = None
        self.port_UDP = None
        self.port_TCP = None


class Clients:
    def __init__(self):
        self.id = None
        self.ip = None
        self.id_comm = None
        self.all_elements = None
        self.element_and_value = [[]]
        self.status = None
        self.udp_port = None
        self.tcp_port = None
        self.udp_socket = None
        self.consecutive_alive = 0
        self.receive_alive = False
        self.first_alive_recv = False


class Socket:
    def __init__(self):
        self.udp_socket = None
        self.tcp_socket = None
        self.connection_socket = None


# GLobal Functions
allowed_client = []
debug_mode = False
sock = Socket()
server_inf = Server()


# ----------------------------SOCKETS------------------------------------------#
def udp_socket():
    # Funcio que obra socket amb el port UDP de les dades del servidor
    global server_inf
    global sock
    sock = Socket()
    sock.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.udp_socket.bind(("", server_inf.port_UDP))


def client_udp_socket(client):
    # Funcio que obra socket amb el port UDP aleatori per intercambiar el REG_INFO amb el client

    global server_inf
    client.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.udp_socket.bind(("", 0))
    ip, port = client.udp_socket.getsockname()

    if (debug_mode == True):
        print_message("Port UDP : " + str(port) + " obert al client: " + client.id)

    return port


def tcp_socket():
    # Funcio que obra socket amb el port TCP del servidor
    global sock
    global server_inf
    sock.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.tcp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.tcp_socket.bind(("", server_inf.port_TCP))
    sock.tcp_socket.listen(5)


def client_tcp_socket(client):
    # Funcio que obra socket amb el port TCP rebut del client
    client.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_address = (client.ip, int(client.tcp_port))
    client.tcp_socket.connect(client_address)
    print_message("Iniciant protocol TCP al port: " + str(client.tcp_port) + " per al client: " + client.id)


# -----------------------------REGISTER && ALIVE FUNCTIONS------------------------------------------#
def udp_loop():
    """ Funcio que continuament esta rebent els paquets UDP dels diferents clients
    connectats.Per cada paquet es crea un fil que tractara el paquet segons el seu tipus.
    Per aqui s'haurien de rebre sol REG_REQ i ALIVES """
    global sock
    while (1):
        try:
            sock.udp_socket.settimeout(2)
            recieved_pck_udp, client_ip, client_port = receive_udp_packet(84, sock.udp_socket)
            pack_ack = threading.Thread(target=register_loop, args=(recieved_pck_udp, client_ip, client_port))
            pack_ack.start()
        except socket.error as socketerror:
            """Com a altres funcions, he decidit optar per utilitzar el .settimeout()
            que retorna una exepcio en cas de no rebre res en el temps esmentat.En el nostre cas
            la manera de tractar el error es ignorarlo, ya que rebem una excepcio per cada paquet no rebut"""
            error = "ignore"


def register_loop(recieved_pck_udp, client_ip, client_port):
    """ Funcio cridada per UDP_loop(), tracta cada tipus de paquet(REG_REQ i ALIVE),
    cridant les seves respectives funcions de tractament """
    pack_type = recieved_pck_udp[0]
    if pack_type == REG_REQ:
        register_reg(recieved_pck_udp, client_ip, client_port)
    if pack_type == ALIVE:
        alive_phase(recieved_pck_udp)


def register_reg(received_package, client_ip, client_udp_port):
    """Tracta els paquets REG_REQ que rep, en cas de ser ivalids enviem REG_REJ.
    Si es valid i el client esta en DISCONNECTED,
    pasarem de fase per completar el registre del client"""

    global sock
    client_id_transm = received_package[1].decode().split("\x00")[0]
    client_id_comm = received_package[2].decode().split("\x00")[0]

    if not client_id_and_id_comm_valid(client_id_transm, client_id_comm):
        print_message("ERROR -> Paquet del client: " + client_id_transm + " no valid, enviant REG_REJ....")
        send_pck = reg_rej_pack_maker("Dades incorrectes")
        sock.udp_socket.sendto(send_pck, (client_ip, client_udp_port))
        return

    valid_client = get_client_by_Id(client_id_transm)
    valid_client.ip = client_ip
    valid_client.id_comm = client_id_comm
    valid_client.udp_port = client_udp_port

    if valid_client.status == DISCONNECTED:
        reg_ack_phase(valid_client)
    else:
        """En cas de barrejar estats i rebre un reg_req en un estat incorrecte, 
        tornem el client a DISCONNECTED"""

        print_message("Rebut paquet REG_REQ en estat diferent a Disconnected")
        change_status(valid_client.id, DISCONNECTED)


def reg_ack_phase(valid_client):
    """Tractament complet del REG_REQ"""
    global sock
    # Creo id_comm i nou port per on iniciarem l'intercanvi del REG_INFO
    rand_num = random.randint(1111111111, 9999999999)
    valid_client.id_comm = str(rand_num)

    # Obrim socket per rebre el REG_INFO
    port_aleatori = client_udp_socket(valid_client)

    # Envio REG_ACK amb el port creat anteriorment
    send_pck = reg_ack_pack_maker(rand_num, port_aleatori)
    send_udp_packet(valid_client, send_pck)
    change_status(valid_client.id, WAIT_INFO)

    """Aqui obtenim el REG_INFO amb el socket obert anteriorment, 
    i li afegim un temps Z * T per rebrel"""
    try:
        valid_client.udp_socket.settimeout(Z * T)
        reg_info, client_ip, client_port = receive_udp_packet(84, valid_client.udp_socket)
    except socket.error as socketerror:
        print_message("Paquet REG_INFO no rebut")
        valid_client.udp_socket.close()
        change_status(valid_client.id, DISCONNECTED)
        return
    valid_client.udp_socket.close()
    # Tanquem socket i tractem el REG_INFO

    pack_type = reg_info[0]
    client_id_comm = reg_info[2].decode().split("\x00")[0]
    client_dades = reg_info[3].decode(errors="ignore").split("\x00")[0]

    # Tractament del REG_INFO
    if pack_type == REG_INFO and client_ip == valid_client.ip and client_id_comm == valid_client.id_comm \
            and len(client_dades) > 10 and valid_client.status == WAIT_INFO:

        set_reg_client_data(valid_client, client_dades)
        send_info_ack = info_ack_pack_maker(valid_client.id_comm)
        send_udp_packet(valid_client, send_info_ack)
        change_status(valid_client.id, REGISTERED)

        # Si completem registre, obrim fil del controlador periodic de comunicació amb el client (wait_alive())
        alive_timeout = datetime.now() + timedelta(seconds=3)
        alive_treatment = threading.Thread(target=wait_alives, args=(valid_client.id, alive_timeout))
        alive_treatment.start()
        return
    else:
        print_message("Paquet REG_INFO incorrecte")
        send_info_nack = info_nack_pack_maker("Dades incorrectes", valid_client.id_comm)
        send_udp_packet(valid_client, send_info_nack)
        change_status(valid_client.id, DISCONNECTED)
        return


def alive_phase(recv_pack):
    """Funcio que tracta els paquets ALIVES rebuts per l'UDP_loop.
    La variable self.first_alive_recv controla que el primer paquet s'hagui rebut,
    i la self.receive_alive serveix per sincronitzar l'estat amb el fil wait_alives,
    per saber si rep ALIVES en el temps demanat"""

    client_id_transm = recv_pack[1].decode().split("\x00")[0]
    client_id_comm = recv_pack[2].decode().split("\x00")[0]
    client_dades = recv_pack[3].decode("latin-1").split("\x00")[0]
    alive_client = get_client_by_Id(client_id_transm)

    if alive_client.id_comm == client_id_comm and len(client_dades) == 0:
        if alive_client.status == REGISTERED:
            alive_client.first_alive_recv = True
        alive_client.receive_alive = True
        alive_pck = alive_pack_maker(alive_client.id, alive_client.id_comm)
        send_udp_packet(alive_client, alive_pck)
    elif alive_client.status == REGISTERED:
        print("INFO -> 1r Alive no valid: ", alive_client.id)
        alive_rej_pck = alive_rej_pack_maker(alive_client.id, alive_client.id_comm)
        change_status(alive_client.id, DISCONNECTED)
        return


def wait_alives(client_id, alive_timeout):
    """Aquesta funcio controla la comunicacio periodica del client.
    Com hem esmentat anteriorment, self.receive_alive controla si el paquet es rep en el minim de temps,
    si no es rep es suma 1 a ALIVES consecutius no rebuts( alive_client.consecutive_alive ).
    Als tres ALIVES no rebuts, client pasa a DISCONNECTED"""

    alive_client = get_client_by_Id(client_id)
    if alive_client.status == REGISTERED:
        """Controlem si el primer paquet s'ha rebut,
        si no es rep client es pasa a DISCONNECTED"""
        received_first_packet = False
        while datetime.now() < alive_timeout:
            if alive_client.first_alive_recv == True and alive_client.receive_alive:
                received_first_packet = True
                change_status(client_id, SEND_ALIVE)
                break
        if not received_first_packet:
            print_message("Primer Paquet ALIVE ha arribat a temps")
            change_status(client_id, DISCONNECTED)
            return

    while alive_client.status == SEND_ALIVE:
        send_alive_timeout = datetime.now() + timedelta(seconds=2)
        alive_client.receive_alive = False
        received_alive = False
        while datetime.now() < send_alive_timeout:
            if alive_client.receive_alive:
                alive_client.consecutive_alive = 0
                received_alive = True
        if not received_alive:
            alive_client.consecutive_alive += 1

        if alive_client.consecutive_alive == 3:
            print("No s'han rebut 3 ALIVE consecutius del client: ", alive_client.id)
            alive_client.consecutive_alive = 0
            change_status(client_id, DISCONNECTED)
            return


# -----------------------------TCP && SET/GET FUNCTIONS------------------------------------------#
def tcp_loop():
    """ Funcio que continuament esta rebent els paquets TCP dels diferents clients
    connectats. Per cada paquet es crea un fil que tractara el paquet.
    Per aqui s'haurien de rebre SEND_DATA"""
    global sock
    while 1:
        try:
            sock.tcp_socket.settimeout(3)
            private_socket, (client_ip, client_port) = sock.tcp_socket.accept()
            received_pck_tcp = receive_tcp_packet(127, private_socket)

            if received_pck_tcp:
                pack_procedure = threading.Thread(target=tcp_pack_procedure, args=(received_pck_tcp, private_socket))
                pack_procedure.start()
        except socket.error as socket_error:
            socket_error = "error"


def tcp_pack_procedure(packet, sockets):
    """ Funcio feta fil per TCP_LOOP(), tracta els SEND_DATA.
    SI rebes algun tipus de paquet incorrecte no el tractaria"""

    client_id = packet[1].decode().split("\x00")[0]
    if packet[0] == SEND_DATA:
        send_data_procedure(packet, sockets)


# -----------------------------SEND && RECEIVE UDP/TCP------------------------------------------#
def send_udp_packet(client, packet):
    #Funcio per enviar paquets amb UDP
    sock.udp_socket.sendto(packet, (client.ip, client.udp_port))

    received_package_unpacked = struct.unpack('B11s11s61s', packet)
    pack_type = received_package_unpacked[0]
    client_id_transm = received_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = received_package_unpacked[2].decode().split("\x00")[0]
    client_dades = received_package_unpacked[3].decode(errors="ignore").split("\x00")[0]

    if debug_mode:
        print_message("UDP PACKET Send -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Dades: " + client_dades)


def receive_udp_packet(num_bytes, sockets):
    #Funcio per rebre paquets amb UDP
    global sock
    global debug_mode
    received_pck_udp, (client_ip, client_port) = sockets.recvfrom(num_bytes)
    received_package_unpacked = struct.unpack('B11s11s61s', received_pck_udp)
    pack_type = received_package_unpacked[0]
    client_id_transm = received_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = received_package_unpacked[2].decode().split("\x00")[0]
    client_dades = received_package_unpacked[3].decode(errors="ignore").split("\x00")[0]

    if debug_mode:
        print_message("UDP PACKET Recv -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Dades: " + client_dades)
    return received_package_unpacked, client_ip, client_port


def receive_tcp_packet(num_bytes, sockets):
    #Funcio per rebre paquets amb TCP
    global sock
    global debug_mode
    recieved_pck_tcp = sockets.recv(num_bytes)

    if recieved_pck_tcp == b'':
        print("Paquet TCP no arribat")
        return False

    received_package_unpacked = struct.unpack('B11s11s8s16s80s', recieved_pck_tcp)
    pack_type = received_package_unpacked[0]
    client_id_transm = received_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = received_package_unpacked[2].decode().split("\x00")[0]
    client_element = received_package_unpacked[3].decode().split("\x00")[0]
    client_valor = received_package_unpacked[4].decode(errors="ignore").split("\x00")[0]
    client_info = received_package_unpacked[5].decode(errors="ignore").split("\x00")[0]

    if debug_mode:
        print_message("TCP PACKET Recv -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Element: " + client_element + ", " +
                      "valor: " + client_valor + ", " +
                      "info: " + client_info)
    return received_package_unpacked


def send_tcp_packet(packet, sockets):
    #Funcio per enviar paquets amb TCP
    global sock
    global debug_mode

    sockets.sendall(packet)

    send_package_unpacked = struct.unpack('B11s11s8s16s80s', packet)
    pack_type = packet[0]
    client_id_transm = send_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = send_package_unpacked[2].decode().split("\x00")[0]
    client_element = send_package_unpacked[3].decode().split("\x00")[0]
    client_valor = send_package_unpacked[4].decode(errors="ignore").split("\x00")[0]
    client_info = send_package_unpacked[5].decode(errors="ignore").split("\x00")[0]

    if debug_mode:
        print_message("TCP PACKET Send -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Element: " + client_element + ", " +
                      "valor: " + client_valor + ", " +
                      "info: " + client_info)
    return


# -----------------------------ERROR CONTROL && PACKET MAKERS------------------------------------------#
"""En aquesta seccio trobarem tots el paquets 
que revisen els condicionals de les funcions 
o constructors de paquets per enviar durant 
el proces de registre o paquets per enviar amb TCP"""

def client_id_and_id_comm_valid(client_id, client_id_comm):
    if get_client_by_Id(client_id):
        return client_id_comm == "0000000000"
    return False


def random_comm_and_id_valid(client, client_id_comm, client_id):
    if client.id_comm == client_id_comm:
        return client_id == client.ip
    return False


def reg_ack_pack_maker(random_comm, client_port):
    reg_ack_pck = struct.pack('B11s11s61s', REG_ACK, bytes(server_inf.id, 'ascii'), bytes(str(random_comm), 'ascii'),
                              bytes(str(client_port), 'ascii'))
    return reg_ack_pck


def info_ack_pack_maker(id_comm):
    reg_ack_pck = struct.pack('B11s11s61s', INFO_ACK, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(str(server_inf.port_TCP), 'ascii'))
    return reg_ack_pck


def reg_rej_pack_maker(motiu):
    reg_ack_pck = struct.pack('B11s11s61s', REG_REJ, bytes(server_inf.id, 'ascii'), bytes("0000000000", 'ascii'),
                              bytes(motiu, 'ascii'))
    return reg_ack_pck


def info_nack_pack_maker(motiu, id_comm):
    reg_ack_pck = struct.pack('B11s11s61s', INFO_NACK, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(motiu, 'ascii'))
    return reg_ack_pck


def alive_pack_maker(client_id, id_comm):
    reg_ack_pck = struct.pack('B11s11s61s', ALIVE, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(client_id, 'ascii'))
    return reg_ack_pck


def alive_rej_pack_maker(client_id, id_comm):
    reg_ack_pck = struct.pack('B11s11s61s', ALIVE_REJ, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(client_id, 'ascii'))
    return reg_ack_pck


def data_ack_pack_maker(client_id, id_comm, element, valor):
    reg_ack_pck = struct.pack('B11s11s8s16s80s', DATA_ACK, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(str(element), 'ascii'), bytes(str(valor), 'ascii'), bytes(client_id, 'ascii'))
    return reg_ack_pck


def data_rej_pack_maker(motiu):
    reg_ack_pck = struct.pack('B11s11s8s16s80s', DATA_REJ, bytes(server_inf.id, 'ascii'), bytes("", 'ascii'),
                              bytes("", 'ascii'), bytes("", 'ascii'), bytes(motiu, 'ascii'))
    return reg_ack_pck


def set_data_maker(client_id, id_comm, element, valor):
    reg_ack_pck = struct.pack('B11s11s8s16s80s', SET_DATA, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(str(element), 'ascii'), bytes(str(valor), 'ascii'), bytes(client_id, 'ascii'))
    return reg_ack_pck


def get_data_pack_maker(client_id, id_comm, element):
    reg_ack_pck = struct.pack('B11s11s8s16s80s', GET_DATA, bytes(server_inf.id, 'ascii'), bytes(str(id_comm), 'ascii'),
                              bytes(str(element), 'ascii'), bytes("", 'ascii'), bytes(client_id, 'ascii'))
    return reg_ack_pck


def element_exist(client, element):
    token = client.all_elements.split(";")

    for true_element in token:
        if element == true_element:
            return True
    return False


# -----------------------------TCP DATA TREATMENT------------------------------------------#
def send_data_procedure(data_pack, socket):
    """Tracta els SEND_DATA rebuts via TCP pel port del client.
    Si tot es correcte la funcio data_maker crea i guarda els diferents canvies en els elements en un "client_id".data"""
    client_id = data_pack[1].decode().split("\x00")[0]
    client_id_comm = data_pack[2].decode().split("\x00")[0]
    client_element = data_pack[3].decode().split("\x00")[0]
    client_valor = data_pack[4].decode("latin-1").split("\x00")[0]

    data_client = get_client_by_Id(client_id)
    if not data_client:
        for clients in allowed_client:
            if clients.id_comm == client_id_comm:
                send_package = data_rej_pack_maker("ID del client incorrecte")
                socket.send(send_package)
                change_status(clients.id, DISCONNECTED)
        return

    if client_id_comm == data_client.id_comm and element_exist(data_client, client_element):
        data_maker(client_id, str_status(data_pack[0]), client_element, client_valor, 0)
        send_package = data_ack_pack_maker(client_id, client_id_comm, client_element, client_valor)
        socket.send(send_package)
        return
    else:
        send_package = data_rej_pack_maker("Dades del dispositiu incorrectes")
        socket.send(send_package)
        change_status(client_id, DISCONNECTED)
        return


def set_call(argument):
    """Tracta la comanda set del servidor. Si rep resposta, pasa a la funcio set_get_rec_procedure que tractara el paquet rebut.
    Si no rep resposta es considera com que el client ha perdut contacte i es pasa a DISCONNECTED"""

    if len(argument.split(" ")) != 4:
        print("Us de la comanda: set <identificador_dispositiu> <identificador_element> <nou_valor>")
        return
    client_id = argument.split(" ")[1]
    element = argument.split(" ")[2]
    valor_element = argument.split(" ")[3]
    client = get_client_by_Id(client_id)

    if client is not False and element_exist(client, element):
        if element.split("-")[2] == "I":
            client_tcp_socket(client)
            send_pck = set_data_maker(client_id, client.id_comm, element, valor_element)
            send_tcp_packet(send_pck, client.tcp_socket)

            try:
                client.tcp_socket.settimeout(M)
                recv_packet = receive_tcp_packet(127, client.tcp_socket)
                set_get_rec_procedure(recv_packet, client, element, SET_DATA)
                client.tcp_socket.close()
                return

            except socket.error as socketerror:
                """Tractem l'excepcio del .settimeout com a resposta no rebuda 
                i canviem client a DISCONNECTED"""

                client.tcp_socket.close()
                print_message("Resposta al SET_DATA no rebuda")
                change_status(client_id, DISCONNECTED)
                return
        print_message("L'element anomenat: " + element + " és un sensor i no permet establir el seu valor")
        return
    else:
        print_message("SET_DATA no realitzat: Dades incorrectes o estat diferent a SEND_ALIVE")
    print("Us de la comanda: set <identificador_dispositiu> <identificador_element> <nou_valor>")


def get_call(argument):
    """Tracta la comanda get del servidor. Si rep resposta, pasa a la funcio set_get_rec_procedure que tractara el paquet rebut.
    Si no rep resposta es considera com que el client ha perdut contacte i es pasa a DISCONNECTED"""

    if len(argument.split(" ")) != 3:
        print("Us de la comanda: get <identificador_dispositiu> <identificador_element>")
        return

    client_id = argument.split(" ")[1]
    element = argument.split(" ")[2]

    client = get_client_by_Id(client_id)

    if client is not False and client.status == SEND_ALIVE and element_exist(client, element):

        client_tcp_socket(client)
        send_pck = get_data_pack_maker(client.id, client.id_comm, element)
        send_tcp_packet(send_pck, client.tcp_socket)

        try:
            client.tcp_socket.settimeout(M)
            recv_packet = receive_tcp_packet(127, client.tcp_socket)
            set_get_rec_procedure(recv_packet, client, element, GET_DATA)
            client.tcp_socket.close()
            return

        except socket.error as socketerror:
            #Mateixa explicacio que el set_call
            client.tcp_socket.close()
            print_message("Resposta al GET_DATA no rebuda")
            change_status(client_id, DISCONNECTED)
            return
    else:
        print_message("GET_DATA no realitzat: Dades incorrectes o estat diferent a SEND_ALIVE")
    print("Us de la comanda: get <identificador_dispositiu> <identificador_element>")


def set_get_rec_procedure(recv_packet, client, element, procedure_type):
    """Tracta els tres tipus de resposta de set i get (DATA_ACK, DATA_NACK, DATA_REJ)"""
    pack_type = recv_packet[0]
    client_id = recv_packet[1].decode().split("\x00")[0]
    client_id_comm = recv_packet[2].decode().split("\x00")[0]
    element_recv = recv_packet[3].decode().split("\x00")[0]
    valor = recv_packet[4].decode(errors="ignore").split("\x00")[0]
    client_info = recv_packet[5].decode(errors="ignore").split("\x00")[0]

    if client.id == client_id and client.id_comm == client_id_comm and element == element_recv:
        if pack_type == DATA_ACK:
            data_maker(client_id, str_status(procedure_type), element, valor, client_info)
            print_message("Operacio: " + str_status(procedure_type) + " correcte, element :" + element + " actualitzat")
        elif pack_type == DATA_NACK:
            print_message(
                "Operacio: " + str_status(procedure_type) + " fallida, element :" + element + " no actualitzat")
        elif pack_type == DATA_REJ:
            change_status(client_id, DISCONNECTED)
        return


# -----------------------------SAVE_FILES AND GLOBAL FUNCTIONS------------------------------------------#

def data_maker(client_id, pack_type, element, value, time_from_client):
    """Crea .data i guarda totes les actualitzacions
    dels element rebuts per send (client), set i get"""

    name_file = str(client_id) + ".data"
    file = open(name_file, "a+")
    file.read()

    if len(value) == 0:
        value = "NO-VALUE"

    current_time = time.strftime("%Y-%m-%d;%H:%M:%S", time.localtime(time.time()))
    if time_from_client != 0:
        current_time = time_from_client
    file.write(current_time + ";" + pack_type + ";" + element + ";" + value)
    file.write('\n')
    file.close()

    print_message("Paquet : " + pack_type + " rebut, " + " Element: " + element + " actualitzat")
    return


def set_reg_client_data(client, data):
    """Guarda la info rebuda per REG_INFO"""
    token = data.split(",")
    client.tcp_port = token[0]
    client.all_elements = token[1]


def arg_treatment(argv):
    """Tracta tots els arguments rebuts al executar el codi via terminal"""

    config_file = None
    allowed_clients = None
    for i in range(len(argv)):
        if argv[i] == "-d":
            global debug_mode
            debug_mode = True
            print_message("Debug mode activat")
        elif argv[i] == "-c" and len(argv) > i + 1:
            try:
                config_file = open(argv[i + 1], 'r')
            except IOError:
                print_message("Error al obrir la configuració del server, utilitzant server.cfg per defecte")
        elif argv[i] == "-u" and len(argv) > i + 1:
            try:
                allowed_clients = open(argv[i + 1], 'r')
            except IOError:
                print_message("Error al obrir la base de dades, utilitzant bbdd_dev.dat per defecte")
    if debug_mode:
        print_message("Llegint configuracions...")

    if config_file is None:
        try:
            config_file = open("server.cfg", 'r')
        except IOError:
            print_message("Error al obrir client.cfg ")
            exit(1)

    if allowed_clients is None:
        try:
            allowed_clients = open("bbdd_dev.dat", 'r')
        except IOError:
            print_message("Error al obrir la base de dades dels clients amb permisos")
            exit(1)

    save_client_database(allowed_clients)
    save_config(config_file)
    if debug_mode:
        print_message("LLegint configuracions i permisos")


def print_message(message):
    #Print missatges amb hora al inici del missatge
    current_time = time.strftime("%H:%M:%S", time.localtime(time.time()))
    print(str(current_time) + " MSG: " + message)
    sys.stdout.flush()


def save_client_database(database_file):
    """Guarda tota la configuracio dels clients
    de la base de dades rebuda com argument"""
    global allowed_client
    num_clients = 0
    for line in database_file:
        if line != "\n":
            token = line.split("\n")[0].split(" ")[0]
            client = Clients()
            client.id = token
            client.status = DISCONNECTED
            client.ip = client.ip
            client.all_elements = client.all_elements
            client.id_comm = client.id_comm
            allowed_client.append(client)
            num_clients += 1
    database_file.close()


def save_config(config_file):
    """Guarda la configuracio del archiu .cfg del servidor"""
    global server_inf

    for line in config_file:
        if line != "\n":
            argument = line.split("\n")[0].split(" ")
            if argument[0] == "Id":
                server_inf.id = argument[2]
            elif argument[0] == "UDP-port":
                server_inf.port_UDP = int(argument[2])
            elif argument[0] == "TCP-port":
                server_inf.port_TCP = int(argument[2])
    config_file.close()


def client_list():
    """Mostra per terminal la llista de clients, amb l'estat, ip, id_comm i elements de cadascun"""
    if allowed_client:
        print("    ID     -     IP      -  ID.COMM   -    ESTAT     -        ELEMENTS        -")
        print("-----------|-------------|------------|--------------|-------------------------")
        for client in allowed_client:
            print(client.id, "-", str(9 * " " if client.ip is None else client.ip),
                  "  -", str(10 * " " if client.id_comm is None else client.id_comm),
                  "-", str_status(client.status), str((11 - len(str_status(client.status))) * " "), "-",
                  str(10 * " " if client.all_elements is None else client.all_elements))


def str_status(status):
    """Segons el tipus de paquet/ estat del client rebut el torna en forma de string.
    Si rep un tipus incorrecte, retorna ERR_TYPE"""
    if status == DISCONNECTED:
        return "DISCONNECTED"
    elif status == NOT_REGISTERED:
        return "NOT_REGISTERED"
    elif status == WAIT_ACK_REG:
        return "WAIT_ACK_REG"
    elif status == WAIT_INFO:
        return "WAIT_INFO"
    elif status == WAIT_ACK_INFO:
        return "WAIT_ACK_INFO"
    elif status == REGISTERED:
        return "REGISTERED"
    elif status == SEND_ALIVE:
        return "SEND_ALIVE"
    elif status == ALIVE:
        return "ALIVE"
    elif status == REG_ACK:
        return "REG_ACK"
    elif status == REG_REQ:
        return "REG_REQ"
    elif status == REG_NACK:
        return "REG_NACK"
    elif status == REG_REJ:
        return "REG_REJ"
    elif status == REG_INFO:
        return "REG_INFO"
    elif status == INFO_ACK:
        return "INFO_ACK"
    elif status == INFO_NACK:
        return "INFO_NACK"
    elif status == INFO_REJ:
        return "INFO_REJ"
    elif status == ALIVE_NACK:
        return "ALIVE_NACK"
    elif status == ALIVE_REJ:
        return "ALIVE_REJ"
    elif status == SEND_DATA:
        return "SEND_DATA"
    elif status == DATA_ACK:
        return "DATA_ACK"
    elif status == DATA_NACK:
        return "DATA_NACK"
    elif status == DATA_REJ:
        return "DATA_REJ"
    elif status == SET_DATA:
        return "SET_DATA"
    elif status == GET_DATA:
        return "GET_DATA"
    else:
        return "ERR_TYPE"


def command_treatment():
    """Tracta les comandes rebudes per terminal"""
    global sock
    try:
        while True:
            line = sys.stdin.readline().strip('\n')
            command = line.split(" ")[0]
            if command == "list":
                client_list()
            elif command == "get":
                get_call(line)
            elif command == "set":
                set_call(line)
            elif command == "quit":
                """Tanquem tots els sockets oberts i enviem senyal SIGTERM als fils per matarlos 
                i poder iniciar el server una altra vegada sense rebre errors dels sockets"""

                print_message("Tancant server...")
                sock.udp_socket.close()
                sock.tcp_socket.close()
                os.kill(os.getpid(), signal.SIGTERM)
            else:
                print_message("ERROR! Comanda no trobada: " + command)
    except(KeyboardInterrupt, SystemExit):
        return


def get_client_by_Id(id_transm):
    """Retorna el client segons una id donada"""
    global allowed_client

    for client in allowed_client:
        if client.id == id_transm:
            return client
    return False


def change_status(client_id, new_state):
    """Canvia el estat del client donat al estat rebut com argument"""
    client = get_client_by_Id(client_id)
    client.status = new_state

    print_message("SERVER -> Client " + client_id + " cambia al estat: "
                  + str_status(new_state))


# -----------------------------MAIN FUNCTION------------------------------------------#
def main():
    global sock
    global allowed_client
    try:
        #Lleguim arguments
        arg_treatment(sys.argv)
        #Creem fil que tractara comandes
        command_thread = threading.Thread(target=command_treatment)
        command_thread.daemon = True
        command_thread.start()
        #Obrim sockets TCP i UDP
        udp_socket()
        tcp_socket()
        #Obrim els UDP_loop i TCP_loop per tractar els paquets dels clients
        udp_connection = threading.Thread(target=udp_loop)
        udp_connection.start()
        tcp_connection = threading.Thread(target=tcp_loop)
        tcp_connection.start()

    except(KeyboardInterrupt, SystemExit):
        #En cas d'error a l'hora dexecutar el programa tancarem tots els sockets
        print_message("Error al executar el servidor, tancant...")
        sock.udp_socket.close()
        sock.tcp_socket.close()
        exit(1)


if __name__ == "__main__":
    main()
