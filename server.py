import os
import random
import select
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
        self.tcp_port = None
        self.udp_socket = None
        self.consecutive_alive = 0

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


def client_udp_socket(client, udp_port):
    global server_inf
    client.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.udp_socket.bind(("", udp_port))

    if (debug_mode == True):
        print("Port UDP :", udp_port, "obert al client:", client.id)


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
        try:
            sock.udp_socket.settimeout(2)
            recieved_pck_udp, client_ip, client_port = recieve_udp_packet(84, sock.udp_socket)
            pack_type = recieved_pck_udp[0]
            if pack_type == REG_REQ:
                register_reg(recieved_pck_udp, client_ip, client_port)
            if pack_type == ALIVE:
                alive_phase(recieved_pck_udp)
        except socket.error as socketerror:
            error = "ignore"


def register_reg(received_package, client_ip, client_udp_port):
    client_id_transm = received_package[1].decode().split("\x00")[0]
    client_id_comm = received_package[2].decode().split("\x00")[0]

    if not client_id_and_id_comm_valid(client_id_transm, client_id_comm):
        print_message("ERROR -> Paquet del client: " + client_id_transm + " no valid")
        send_pck = reg_rej_pack_maker("Dades incorrectes")
        sock.udp_socket.sendto(send_pck, (client_ip, client_udp_port))
        print("No valid")
        return

    valid_client = get_client_by_Id(client_id_transm)
    valid_client.ip = client_ip
    valid_client.id_comm = client_id_comm
    valid_client.udp_port = client_udp_port

    if valid_client.status == DISCONNECTED:
        reg_ack_phase(valid_client)


def reg_ack_phase(valid_client):
    udp_port = 46666
    rand_num = random.randint(1111111111, 9999999999)
    send_pck = reg_ack_pack_maker(rand_num, udp_port)
    change_client_state(valid_client.id, WAIT_INFO)
    valid_client.id_comm = str(rand_num)
    sock.udp_socket.sendto(send_pck, (valid_client.ip, valid_client.udp_port))
    client_udp_socket(valid_client, udp_port)

    trys_recv_reg_info = 0
    while valid_client.status == WAIT_INFO and trys_recv_reg_info != Z*3:
        try:
            valid_client.udp_socket.settimeout(2)
            reg_info, client_ip, client_port = recieve_udp_packet(84, valid_client.udp_socket)
            if reg_info[0] == REG_INFO:
                break
            trys_recv_reg_info += 1
        except socket.error as socketerror:
            trys_recv_reg_info += 1

    if trys_recv_reg_info == Z * 3:
        change_client_state(valid_client.id, DISCONNECTED)
        print_message("ERROR-> PAQUET REG INFO no rebut, passant a DISCONNECTED")
        return

    valid_client.udp_socket.close()
    pack_type = reg_info[0]
    client_id_comm = reg_info[2].decode().split("\x00")[0]
    client_dades = reg_info[3].decode(errors="ignore").split("\x00")[0]

    if pack_type == REG_INFO and client_ip == valid_client.ip and client_id_comm == valid_client.id_comm and len(client_dades) > 10 and valid_client.status == WAIT_INFO:
        set_reg_client_data(valid_client, client_dades)
        send_info_ack = info_ack_pack_maker(valid_client.id_comm)
        sock.udp_socket.sendto(send_info_ack, (valid_client.ip, valid_client.udp_port))
        change_client_state(valid_client.id, REGISTERED)

        first_alive = threading.Thread(target=wait_first_alive, args=(valid_client.id,))
        first_alive.start()
        return
    else:
        send_info_nack = info_nack_pack_maker("Dades incorrectes", valid_client.id_comm)
        sock.udp_socket.sendto(send_info_nack, (valid_client.ip, valid_client.udp_port))
        change_client_state(valid_client.id, DISCONNECTED)

def alive_phase(recv_pack):
    client_id_transm = recv_pack[1].decode().split("\x00")[0]
    client_id_comm = recv_pack[2].decode().split("\x00")[0]
    client_dades = recv_pack[3].decode("latin-1").split("\x00")[0]

    alive_client = get_client_by_Id(client_id_transm)

    print(client_dades, " de llargaria: ", len(client_dades))

    if alive_client.status == REGISTERED and alive_client.id_comm == client_id_comm:
        change_client_state(alive_client.id, SEND_ALIVE)
    elif alive_client.status == REGISTERED:
        print("INFO -> 1r Alive no valid: ", alive_client.id)
        alive_rej_pck = alive_rej_pack_maker(alive_client.id, alive_client.id_comm)
        change_client_state(alive_client.id, DISCONNECTED)

    if alive_client.status == SEND_ALIVE and alive_client.id_comm == client_id_comm:
        alive_pck = alive_pack_maker(alive_client.id, alive_client.id_comm)
        sock.udp_socket.sendto(alive_pck, (alive_client.ip, alive_client.udp_port))

        if alive_client.consecutive_alive > 0 and alive_client.consecutive_alive < X-1:
            alive_client.consecutive_alive -= 1
        return

    alive_client.consecutive_alive += 1
    print("INFO -> Alives no consecutius: ", alive_client.consecutive_alive)
    if alive_client.consecutive_alive == X:
        print("INFO-> No s'ha rebut 3 ALIVES consecutius de: ", alive_client.id)
        change_client_state(alive_client.id, DISCONNECTED)
        return


def wait_first_alive(client_id):
    client = get_client_by_Id(client_id)
    time.sleep(W)
    if(client.status == REGISTERED):
        print("INFO -> Primer ALIVE no rebut en 3 segons...Tornant ", client.id, "a l'estat DISCONNECTED")
        change_client_state(client.id, DISCONNECTED)
        return


# -----------------------------SEND && RECIEVE UDP/TCP------------------------------------------#

def recieve_udp_packet(num_bytes, sockets):
    global sock
    global debug_mode
    recieved_pck_udp, (client_ip, client_port) = sockets.recvfrom(num_bytes)
    received_package_unpacked = struct.unpack('B11s11s61s', recieved_pck_udp)
    pack_type = received_package_unpacked[0]
    client_id_transm = received_package_unpacked[1].decode().split("\x00")[0]
    client_id_comm = received_package_unpacked[2].decode().split("\x00")[0]
    client_dades = received_package_unpacked[3].decode(errors="ignore").split("\x00")[0]

    if debug_mode == True:
        print_message("UDP PACKET Recv -> Type: " + str_status(pack_type) + ", " +
                      "Id_transm: " + client_id_transm + ", " +
                      "Id_comm: " + client_id_comm + ", " +
                      "Dades: " + client_dades)
    return received_package_unpacked, client_ip, client_port


# -----------------------------ERROR CONTROL && PACKET MAKERS------------------------------------------#

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


# -----------------------------SAVE_FILES AND GLOBAL FUNCTIONS------------------------------------------#
def set_reg_client_data(client, data):
    data = "6359,LUM-0-O;LUM-0-I;PRE-0-O;TEM-0-O"
    token = data.split(",")
    client.tcp_socket = token[0]
    client.elements = token[1]


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

def change_client_state(client_id, new_state):
    client = get_client_by_Id(client_id)
    client.status = new_state

    print_message("INFO  -> Client " + client_id + " cambia al estat: "
                  + str_status(new_state))

# -----------------------------MAIN FUNCTION------------------------------------------#
def main():
    arg_treatment(sys.argv)
    command_thread = threading.Thread(target=command_treatment)
    command_thread.start()
    udp_socket()
    tcp_socket()
    reg_alive_phase_loop = threading.Thread(target=register_loop)
    reg_alive_phase_loop.start()

if __name__ == "__main__":
    main()
