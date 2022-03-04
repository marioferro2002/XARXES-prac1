server_config = []

file = open("server.cfg", "r")
token = file.readline()
token = token.split()

server_config.append(token[2])

token = file.readline()
token = token.split()
server_config.append(token[2])

token = file.readline()
token = token.split()
server_config.append(token[2])

print(server_config[0])




