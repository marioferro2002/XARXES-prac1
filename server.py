import os
import random
import threading
import signal
import socket
import struct
import sys
import threading
import time
from datetime import datetime
from datetime import timedelta


class Server:
    def __init__(self):
        self.id = None
        self.udp_port = None
        self.tcp_port = None

class Sockets:
    def __init__(self):
        self.udp_port = None
        self.tcp_port = None
        self.connect_tcp_port = None

# Global Variables
server_info = None

def get_server_config():

    global server_info
    global sockets
    server_info = Server()
    sock = Sockets()

    file = open("server.cfg", "r")

    token = file.readline()
    token = token.split()
    server_info.id = token[2]

    token = file.readline()
    token = token.split()
    server_info.udp_port = int(token[2])

    token = file.readline()
    token = token.split()
    server_info.tcp_port = int(token[2])

    file.close

def list_accepted_clients():
    if valid_clients_data:
        printing_mutex.acquire()
        clients_data_mutex.acquire()
        print("  NAME |      IP      |      MAC      | RAND NUM |     STATE     ")
        print("-------|--------------|---------------|----------|---------------")
        for client in valid_clients_data:
            print(" " + client.name + " | " + str(13 * " " if client.ip_address is None else
                  client.ip_address + " " * (13 - len(client.ip_address))) + "| " +
                  client.mac_address + "  | " + str(format(client.random_num, "06")) + "   | " +
                  client.state + "")

        print  # simply prints new line
        sys.stdout.flush()
        printing_mutex.release()
        clients_data_mutex.release()
if __name__ == '__main__':
    get_server_config()
