import os
import sys
import ssl
import threading
import urllib.parse
# Rename HTTPServer to _HTTPServer so as to avoid confusion with HTTPSServer.
from http.server import HTTPServer as _HTTPServer, SimpleHTTPRequestHandler

from test import support

here = os.path.dirname(__file__)

HOST = support.HOST
CERTFILE = os.path.join(here, 'keycert.pem')

# This one's based on HTTPServer, which is based on SocketServer

class HTTPSServer(_HTTPServer):

    def __init__(self, server_address, handler_class, certfile):
        _HTTPServer.__init__(self, server_address, handler_class)
        # we assume the certfile contains both private key and certificate
        self.certfile = certfile
        self.allow_reuse_address = True

    def __str__(self):
        return ('<%s %s:%s>' %
                (self.__class__.__name__,
                 self.server_name,
                 self.server_port))

    def get_request(self):
        # override this to wrap socket with SSL
        sock, addr = self.socket.accept()
        sslconn = ssl.wrap_socket(sock, server_side=True,
                                  certfile=self.certfile)
        return sslconn, addr

class RootedHTTPRequestHandler(SimpleHTTPRequestHandler):
    # need to override translate_path to get a known root,
    # instead of using os.curdir, since the test could be
    # run from anywhere

    server_version = "TestHTTPS/1.0"
    root = here

    def translate_path(self, path):
        """Translate a /-separated PATH to the local filename syntax.

        Components that mean special things to the local file system
        (e.g. drive or directory names) are ignored.  (XXX They should
        probably be diagnosed.)

        """
        # abandon query parameters
        path = urllib.parse.urlparse(path)[2]
        path = os.path.normpath(urllib.parse.unquote(path))
        words = path.split('/')
        words = filter(None, words)
        path = self.root
        for word in words:
            drive, word = os.path.splitdrive(word)
            head, word = os.path.split(word)
            path = os.path.join(path, word)
        return path

    def log_message(self, format, *args):
        # we override this to suppress logging unless "verbose"
        if support.verbose:
            sys.stdout.write(" server (%s:%d %s):\n   [%s] %s\n" %
                             (self.server.server_address,
                              self.server.server_port,
                              self.request.cipher(),
                              self.log_date_time_string(),
                              format%args))

class HTTPSServerThread(threading.Thread):

    def __init__(self, certfile, host=HOST, handler_class=None):
        self.flag = None
        self.server = HTTPSServer((host, 0),
                                  handler_class or RootedHTTPRequestHandler,
                                  certfile)
        self.port = self.server.server_port
        threading.Thread.__init__(self)
        self.daemon = True

    def __str__(self):
        return "<%s %s>" % (self.__class__.__name__, self.server)

    def start(self, flag=None):
        self.flag = flag
        threading.Thread.start(self)

    def run(self):
        if self.flag:
            self.flag.set()
        self.server.serve_forever(0.05)

    def stop(self):
        self.server.shutdown()


def make_https_server(case, certfile=CERTFILE, host=HOST, handler_class=None):
    server = HTTPSServerThread(certfile, host, handler_class)
    flag = threading.Event()
    server.start(flag)
    flag.wait()
    def cleanup():
        if support.verbose:
            sys.stdout.write('stopping HTTPS server\n')
        server.stop()
        if support.verbose:
            sys.stdout.write('joining HTTPS thread\n')
        server.join()
    case.addCleanup(cleanup)
    return server
