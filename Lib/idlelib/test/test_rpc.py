import sys
import time
import socket
import unittest
import subprocess
from idlelib import rpc
from idlelib.test import common

# XXX this is not testing anything yet

HOST = '127.0.0.1'
PORT = 0

class SocketIOTest(unittest.TestCase):

    def setUp(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.bind((HOST, PORT))
        client.listen(1)
        port = client.getsockname()[1]
        self.server = subprocess.Popen([sys.executable, "-c",
            """x = __import__('idlelib.test.raw_server')
x.test.raw_server.idle_server(%r, %d)""" % (HOST, port)])
        sock, _ = client.accept()
        self.sockio = rpc.SocketIO(sock, debugging=True)#common.RPC_DEBUGGING)
        self.sockio.location = 'Client'
        print sock.getsockname()

    def tearDown(self):
        self.server.kill()
        self.server.wait()
        self.sockio.close()

    def test_x(self):
        long_list = range(rpc.BUFSIZE * 2)
        self.sockio.putmessage(long_list)
        self.sockio.putmessage(long_list)
        self.sockio.putmessage(long_list)


class RpcTest(unittest.TestCase):

    def setUp(self):
        # XXX will probably have to subclass RPCClient since its handle_EOF
        # calls the exithook method which calls os._exit
        self.client = client = rpc.RPCClient((HOST, PORT))
        client.debugging = common.RPC_DEBUGGING
        listening_sock = client.listening_sock
        listening_sock.settimeout(10)
        port = str(listening_sock.getsockname()[1])
        self.server = subprocess.Popen([sys.executable, "-c",
            "__import__('idlelib.run').run.main()", port])
        client.accept()
        client.register("stdout", sys.stdout)
        client.register("stderr", sys.stderr)

    def tearDown(self):
        self.server.kill()
        self.server.wait()
        self.client.listening_sock.close()

    def test_x(self):
        client, server = self.client, self.server

        active_seq = client.asyncqueue('exec', 'runcode', ('print "hi"', ), {})
        while True:
            response = client.pollresponse(active_seq, wait=0.05)
            if not response:
                time.sleep(0.1)
            else:
                break
        how, what = response
        print active_seq, how, what, "<<"


if __name__ == "__main__":
    unittest.main()
