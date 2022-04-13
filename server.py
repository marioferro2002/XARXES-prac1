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
        self.elements = None
        self.status = None
        self.udp_port = None


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
    global server_inf
    global sock
    sock = Socket()
    sock.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.udp_socket.bind(("", server_inf.port_UDP))


def tcp_socket():
    global sock
    global server_inf
    sock.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.tcp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.tcp_socket.bind(("", server_inf.port_TCP))
    sock.tcp_socket.listen(5)


# -----------------------------REGISTER && ALIVE FUNCTIONS------------------------------------------#

def register_loop():
    global sock
    while (1):
        recieved_pck_udp, client_ip, client_port = recieve_udp_packet(84)
        pack_type = recieved_pck_udp[0]
        if pack_type == REG_REQ:
            register_reg(recieved_pck_udp, client_ip, client_port)


def register_reg(received_package, client_ip, client_udp_port):
    client_id_transm = received_package[1].decode().split("\x00")[0]
    client_id_comm = received_package[2].decode().split("\x00")[0]

    if not client_id_and_id_comm_valid(client_id_transm, client_id_comm):
        print_message("ERROR -> Paquet del client: " + client_id_transm + " no valid")
        return

    valid_client = get_client_by_Id(client_id_transm)
    valid_client.ip = client_ip
    valid_client.id_comm = client_id_comm
    valid_client.udp_port = client_udp_port
    if valid_client.status == DISCONNECTED:
        return


# -----------------------------SEND && RECIEVE UDP/TCP------------------------------------------#

def recieve_udp_packet(num_bytes):
    global sock
    global debug_mode
    recieved_pck_udp, (client_ip, client_port) = sock.udp_socket.recvfrom(num_bytes)
    received_package_unpacked = struct.unpack('B11s11s61s', recieved_pck_udp)
    pack_type = received_package_unpacked[0]
    client_id_transm = received_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = received_package_unpacked[2].decode().split("\x00")[0]
    client_dades = received_package_unpacked[3].decode(errors="ignore").split("\x00")[0]

    if debug_mode == True:
        print_message("UDP PACKET -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Dades: " + client_dades)
    return received_package_unpacked, client_ip, client_port


# -----------------------------ERROR CONTROL && PACKET MAKERS------------------------------------------#

def client_id_and_id_comm_valid(client_id, client_id_comm):
    if get_client_by_Id(client_id):

        return client_id_comm == "0000000000"
    return False

def reg_ack_packer():
    return

# -----------------------------SAVE_FILES AND GLOBAL FUNCTIONS------------------------------------------#

def arg_treatment(argv):
    config_file = None
    allowed_clients = None
    for i in range(len(argv)):
        if argv[i] == "-d":
            global debug_mode
            debug_mode = True
            print_message("INFO -> Debug mode activat")
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
        print_message("DEBUG -> Llegint configuracions...")

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
            print_message("ERROR al obrir els clients amb permisos")
            exit(1)

    save_client_database(allowed_clients)
    save_config(config_file)
    if debug_mode:
        print_message("DEBUG -> LLegint configuracions i permisos")


def print_message(message):
    current_time = time.strftime("%H:%M:%S", time.localtime(time.time()))
    print(str(current_time) + " MSG: " + message)
    sys.stdout.flush()


def save_client_database(database_file):
    global allowed_client
    num_clients = 0
    for line in database_file:
        if line != "\n":
            token = line.split("\n")[0].split(" ")[0]
            client = Clients()
            client.id = token
            client.status = DISCONNECTED
            client.ip = client.ip
            client.elements = client.elements
            client.id_comm = client.id_comm
            allowed_client.append(client)
            num_clients += 1
    database_file.close()


def save_config(config_file):
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
    if allowed_client:
        print("    ID     -     IP      -  ID.COMM   -    ESTAT     -        ELEMENTS        -")
        print("-----------|-------------|------------|--------------|-------------------------")
        for client in allowed_client:
            print(client.id, "-", str(9 * " " if client.ip is None else client.ip),
                  "  -", str(10 * " " if client.id_comm is None else client.id_comm),
                  "-", str_status(client.status), str((11 - len(str_status(client.status))) * " "), "-",
                  str(10 * " " if client.elements is None else client.elements))


def str_status(status):
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
    global sock
    while True:
        line = sys.stdin.readline()
        command = line.split("\n")[0]

        if command == "list":
            client_list()
        elif command == "quit":
            print_message("Tancant server...")
            os.kill(os.getpid(), signal.SIGINT)
        else:
            print_message("ERROR! Comanda no trobada: " + command)


def get_client_by_Id(id_transm):
    global allowed_client

    for client in allowed_client:
        if client.id == id_transm:
            return client
    return False


def main():
    arg_treatment(sys.argv)
    command_thread = threading.Thread(target=command_treatment)
    command_thread.start()
    udp_socket()
    tcp_socket()
    reg_alive_phase_loop = threading.Thread(target=register_loop)
    reg_alive_phase_loop.start()


# -----------------------------MAIN FUNCTION------------------------------------------#
if __name__ == "__main__":
    main()
