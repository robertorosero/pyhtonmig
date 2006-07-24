import socket

def getaddrinfo(host,port, *args):
	"""no real addr info -- but the test data is copied from 'port' to 'sa'
   for a bit of control over the resulting mock socket.
	>>> getaddrinfo("MOCK", "Raise: connection prohibited")
	("af", "socktype", "proto", "cannonname", "Raise: connection prohibited")
   """

	if host != "MOCK":
		raise ValueError("Faked Socket Module for testing only")
	# Use port for sa, so we can fake a raise
	return ("af", "socktype", "proto", "cannonname", port)

# Bad name, but it matches the real module
class error(Exception): pass

class socket(object):
	"""Mock socket object"""
	def __init__(self, af, socktype, proto): pass
	def connect(self, sa):
		"""Raise if the argument says raise, otherwise sa is treated as the response.
		Wouldn't hurt to put doctests in here, too...
		"""
		if sa.startswith("Raise: "):
			raise error(sa)
		else:
			self.incoming_msg = sa
	def close(self): pass
	def sendall(self, msg):
		self.gotsent = msg
	def makefile(self, mode, bufsize):
		return cStringIO(self.incoming_msg)