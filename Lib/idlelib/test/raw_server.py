import sys
import socket
from idlelib import rpc
from idlelib.test import common

def idle_server(host, port):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.connect((host, port))
    sockio = rpc.SocketIO(server, debugging=True)#common.RPC_DEBUGGING)
    sockio.location = 'Server'
    sockio.mainloop()
