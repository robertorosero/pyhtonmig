import sys
import os
import urllib
import urlparse
import plistlib
import distutils.util

_scriptExc_NotInstalled = "pimp._scriptExc_NotInstalled"
_scriptExc_OldInstalled = "pimp._scriptExc_OldInstalled"
_scriptExc_BadInstalled = "pimp._scriptExc_BadInstalled"

NO_EXECUTE=0

DEFAULT_FLAVORORDER=['source', 'binary']
DEFAULT_DOWNLOADDIR='/tmp'
DEFAULT_BUILDDIR='/tmp'
DEFAULT_INSTALLDIR=os.path.join(sys.prefix, "Lib", "site-packages")
DEFAULT_PIMPDATABASE="http://www.cwi.nl/~jack/pimp/pimp-%s.plist" % distutils.util.get_platform()

ARCHIVE_FORMATS = [
	(".tar.Z", "zcat \"%s\" | tar xf -"),
	(".taz", "zcat \"%s\" | tar xf -"),
	(".tar.gz", "zcat \"%s\" | tar xf -"),
	(".tgz", "zcat \"%s\" | tar xf -"),
	(".tar.bz", "bzcat \"%s\" | tar xf -"),
]

class PimpPreferences:
	def __init__(self, 
			flavorOrder=None,
			downloadDir=None,
			buildDir=None,
			installDir=None,
			pimpDatabase=None):
		if not flavorOrder:
			flavorOrder = DEFAULT_FLAVORORDER
		if not downloadDir:
			downloadDir = DEFAULT_DOWNLOADDIR
		if not buildDir:
			buildDir = DEFAULT_BUILDDIR
		if not installDir:
			installDir = DEFAULT_INSTALLDIR
		if not pimpDatabase:
			pimpDatabase = DEFAULT_PIMPDATABASE
		self.flavorOrder = flavorOrder
		self.downloadDir = downloadDir
		self.buildDir = buildDir
		self.installDir = installDir
		self.pimpDatabase = pimpDatabase
		
	def check(self):
		rv = ""
		RWX_OK = os.R_OK|os.W_OK|os.X_OK
		if not os.path.exists(self.downloadDir):
			rv += "Warning: Download directory \"%s\" does not exist\n" % self.downloadDir
		elif not os.access(self.downloadDir, RWX_OK):
			rv += "Warning: Download directory \"%s\" is not writable or not readable\n" % self.downloadDir
		if not os.path.exists(self.buildDir):
			rv += "Warning: Build directory \"%s\" does not exist\n" % self.buildDir
		elif not os.access(self.buildDir, RWX_OK):
			rv += "Warning: Build directory \"%s\" is not writable or not readable\n" % self.buildDir
		if not os.path.exists(self.installDir):
			rv += "Warning: Install directory \"%s\" does not exist\n" % self.installDir
		elif not os.access(self.installDir, RWX_OK):
			rv += "Warning: Install directory \"%s\" is not writable or not readable\n" % self.installDir
		else:
			installDir = os.path.realpath(self.installDir)
			for p in sys.path:
				try:
					realpath = os.path.realpath(p)
				except:
					pass
				if installDir == realpath:
					break
			else:
				rv += "Warning: Install directory \"%s\" is not on sys.path\n" % self.installDir
		return rv			
		
	def compareFlavors(self, left, right):
		if left in self.flavorOrder:
			if right in self.flavorOrder:
				return cmp(self.flavorOrder.index(left), self.flavorOrder.index(right))
			return -1
		if right in self.flavorOrder:
			return 1
		return cmp(left, right)
		
class PimpDatabase:
	def __init__(self, prefs):
		self._packages = []
		self.preferences = prefs
		self._urllist = []
		self._version = ""
		self._maintainer = ""
		self._description = ""
		
	def appendURL(self, url, included=0):
		if url in self._urllist:
			return
		self._urllist.append(url)
		fp = urllib.urlopen(url).fp
		dict = plistlib.Plist.fromFile(fp)
		# Test here for Pimp version, etc
		if not included:
			self._version = dict.get('version', '0.1')
			self._maintainer = dict.get('maintainer', '')
			self._description = dict.get('description', '')
		self.appendPackages(dict['packages'])
		others = dict.get('include', [])
		for url in others:
			self.appendURL(url, included=1)
		
	def appendPackages(self, packages):
		for p in packages:
			pkg = PimpPackage(self, **dict(p))
			self._packages.append(pkg)
			
	def list(self):
		return self._packages
		
	def listnames(self):
		rv = []
		for pkg in self._packages:
			rv.append(_fmtpackagename(pkg))
		return rv
		
	def dump(self, pathOrFile):
		packages = []
		for pkg in self._packages:
			packages.append(pkg.dump())
		dict = {
			'version': self._version,
			'maintainer': self._maintainer,
			'description': self._description,
			'packages': packages
			}
		plist = plistlib.Plist(**dict)
		plist.write(pathOrFile)
		
	def find(self, ident):
		if type(ident) == str:
			# Remove ( and ) for pseudo-packages
			if ident[0] == '(' and ident[-1] == ')':
				ident = ident[1:-1]
			# Split into name-version-flavor
			fields = ident.split('-')
			if len(fields) < 1 or len(fields) > 3:
				return None
			name = fields[0]
			if len(fields) > 1:
				version = fields[1]
			else:
				version = None
			if len(fields) > 2:
				flavor = fields[2]
			else:
				flavor = None
		else:
			name = ident['name']
			version = ident.get('version')
			flavor = ident.get('flavor')
		found = None
		for p in self._packages:
			if name == p.name and \
					(not version or version == p.version) and \
					(not flavor or flavor == p.flavor):
				if not found or found < p:
					found = p
		return found
		
class PimpPackage:
	def __init__(self, db, name, 
			version=None,
			flavor=None,
			description=None,
			longdesc=None,
			downloadURL=None,
			installTest=None,
			prerequisites=None):
		self._db = db
		self.name = name
		self.version = version
		self.flavor = flavor
		self.description = description
		self.longdesc = longdesc
		self.downloadURL = downloadURL
		self._installTest = installTest
		self._prerequisites = prerequisites
		
	def dump(self):
		dict = {
			'name': self.name,
			}
		if self.version:
			dict['version'] = self.version
		if self.flavor:
			dict['flavor'] = self.flavor
		if self.description:
			dict['description'] = self.description
		if self.longdesc:
			dict['longdesc'] = self.longdesc
		if self.downloadURL:
			dict['downloadURL'] = self.downloadURL
		if self._installTest:
			dict['installTest'] = self._installTest
		if self._prerequisites:
			dict['prerequisites'] = self._prerequisites
		return dict
		
	def __cmp__(self, other):
		if not isinstance(other, PimpPackage):
			return cmp(id(self), id(other))
		if self.name != other.name:
			return cmp(self.name, other.name)
		if self.version != other.version:
			return cmp(self.version, other.version)
		return self._db.preferences.compareFlavors(self.flavor, other.flavor)
		
	def installed(self):
		namespace = {
			"NotInstalled": _scriptExc_NotInstalled,
			"OldInstalled": _scriptExc_OldInstalled,
			"BadInstalled": _scriptExc_BadInstalled,
			"os": os,
			"sys": sys,
			}
		installTest = self._installTest.strip() + '\n'
		try:
			exec installTest in namespace
		except ImportError, arg:
			return "no", str(arg)
		except _scriptExc_NotInstalled, arg:
			return "no", str(arg)
		except _scriptExc_OldInstalled, arg:
			return "old", str(arg)
		except _scriptExc_BadInstalled, arg:
			return "bad", str(arg)
		except:
			print 'TEST:', repr(self._installTest)
			return "bad", "Package install test got exception"
		return "yes", ""
		
	def prerequisites(self):
		rv = []
		if not self.downloadURL:
			return [(None, "This package needs to be installed manually")]
		if not self._prerequisites:
			return []
		for item in self._prerequisites:
			if type(item) == str:
				pkg = None
				descr = str(item)
			else:
				pkg = self._db.find(item)
				if not pkg:
					descr = "Requires unknown %s"%_fmtpackagename(item)
				else:
					descr = pkg.description
			rv.append((pkg, descr))
		return rv
			
	def _cmd(self, output, dir, *cmditems):
		cmd = ("cd \"%s\"; " % dir) + " ".join(cmditems)
		if output:
			output.write("+ %s\n" % cmd)
		if NO_EXECUTE:
			return 0
		fp = os.popen(cmd, "r")
		while 1:
			line = fp.readline()
			if not line:
				break
			if output:
				output.write(line)
		rv = fp.close()
		return rv
		
	def downloadSinglePackage(self, output):
		scheme, loc, path, query, frag = urlparse.urlsplit(self.downloadURL)
		path = urllib.url2pathname(path)
		filename = os.path.split(path)[1]
		self.archiveFilename = os.path.join(self._db.preferences.downloadDir, filename)
		if self._cmd(output, self._db.preferences.downloadDir, "curl",
				"--output", self.archiveFilename,
				self.downloadURL):
			return "download command failed"
		if not os.path.exists(self.archiveFilename) and not NO_EXECUTE:
			return "archive not found after download"
			
	def unpackSinglePackage(self, output):
		filename = os.path.split(self.archiveFilename)[1]
		for ext, cmd in ARCHIVE_FORMATS:
			if filename[-len(ext):] == ext:
				break
		else:
			return "unknown extension for archive file: %s" % filename
		basename = filename[:-len(ext)]
		cmd = cmd % self.archiveFilename
		self._buildDirname = os.path.join(self._db.preferences.buildDir, basename)
		if self._cmd(output, self._db.preferences.buildDir, cmd):
			return "unpack command failed"
		setupname = os.path.join(self._buildDirname, "setup.py")
		if not os.path.exists(setupname) and not NO_EXECUTE:
			return "no setup.py found after unpack of archive"
			
	def installSinglePackage(self, output):
		if not self.downloadURL:
			return "%s: This package needs to be installed manually" % _fmtpackagename(self)
		msg = self.downloadSinglePackage(output)
		if msg:
			return "download %s: %s" % (_fmtpackagename(self), msg)
		msg = self.unpackSinglePackage(output)
		if msg:
			return "unpack %s: %s" % (_fmtpackagename(self), msg)
		if self._cmd(output, self._buildDirname, sys.executable, "setup.py install"):
			return "install %s: running \"setup.py install\" failed" % _fmtpackagename(self)
		return None

class PimpInstaller:
	def __init__(self, db):
		self._todo = []
		self._db = db
		self._curtodo = []
		self._curmessages = []
		
	def __contains__(self, package):
		return package in self._todo
		
	def _addPackages(self, packages):
		for package in packages:
			if not package in self._todo:
				self._todo.insert(0, package)
			
	def _prepareInstall(self, package, force=0, recursive=1):
		if not force:
			status, message = package.installed()
			if status == "yes":
				return 
		if package in self._todo or package in self._curtodo:
			return
		self._curtodo.insert(0, package)
		if not recursive:
			return
		prereqs = package.prerequisites()
		for pkg, descr in prereqs:
			if pkg:
				self._prepareInstall(pkg, force, recursive)
			else:
				self._curmessages.append("Requires: %s" % descr)
				
	def prepareInstall(self, package, force=0, recursive=1):
		self._curtodo = []
		self._curmessages = []
		self._prepareInstall(package, force, recursive)
		rv = self._curtodo, self._curmessages
		self._curtodo = []
		self._curmessages = []
		return rv
		
	def install(self, packages, output):
		self._addPackages(packages)
		status = []
		for pkg in self._todo:
			msg = pkg.installSinglePackage(output)
			if msg:
				status.append(msg)
		return status
		
		
def _fmtpackagename(dict):
	if isinstance(dict, PimpPackage):
		dict = dict.dump()
	rv = dict['name']
	if dict.has_key('version'):
		rv = rv + '-%s' % dict['version']
	if dict.has_key('flavor'):
		rv = rv + '-%s' % dict['flavor']
	if not dict.get('downloadURL'):
		# Pseudo-package, show in parentheses
		rv = '(%s)' % rv
	return rv
	
def _run(mode, verbose, force, args):
	prefs = PimpPreferences()
	prefs.check()
	db = PimpDatabase(prefs)
	db.appendURL(prefs.pimpDatabase)
	
	if mode =='list':
		if not args:
			args = db.listnames()
		print "%-20.20s\t%s" % ("Package", "Description")
		print
		for pkgname in args:
			pkg = db.find(pkgname)
			if pkg:
				description = pkg.description
				pkgname = _fmtpackagename(pkg)
			else:
				description = 'Error: no such package'
			print "%-20.20s\t%s" % (pkgname, description)
			if verbose:
				print "\tHome page:\t", pkg.longdesc
				print "\tDownload URL:\t", pkg.downloadURL
	if mode =='status':
		if not args:
			args = db.listnames()
			print "%-20.20s\t%s\t%s" % ("Package", "Installed", "Message")
			print
		for pkgname in args:
			pkg = db.find(pkgname)
			if pkg:
				status, msg = pkg.installed()
				pkgname = _fmtpackagename(pkg)
			else:
				status = 'error'
				msg = 'No such package'
			print "%-20.20s\t%-9.9s\t%s" % (pkgname, status, msg)
			if verbose and status == "no":
				prereq = pkg.prerequisites()
				for pkg, msg in prereq:
					if not pkg:
						pkg = ''
					else:
						pkg = _fmtpackagename(pkg)
					print "%-20.20s\tRequirement: %s %s" % ("", pkg, msg)
	elif mode == 'install':
		if not args:
			print 'Please specify packages to install'
			sys.exit(1)
		inst = PimpInstaller(db)
		for pkgname in args:
			pkg = db.find(pkgname)
			if not pkg:
				print '%s: No such package' % pkgname
				continue
			list, messages = inst.prepareInstall(pkg, force)
			if messages and not force:
				print "%s: Not installed:" % pkgname
				for m in messages:
					print "\t", m
			else:
				if verbose:
					output = sys.stdout
				else:
					output = None
				messages = inst.install(list, output)
				if messages:
					print "%s: Not installed:" % pkgname
					for m in messages:
						print "\t", m

def main():
	import getopt
	def _help():
		print "Usage: pimp [-v] -s [package ...]  List installed status"
		print "       pimp [-v] -l [package ...]  Show package information"
		print "       pimp [-vf] -i package ...   Install packages"
		print "Options:"
		print "       -v  Verbose"
		print "       -f  Force installation"
		sys.exit(1)
		
	try:
		opts, args = getopt.getopt(sys.argv[1:], "slifv")
	except getopt.Error:
		_help()
	if not opts and not args:
		_help()
	mode = None
	force = 0
	verbose = 0
	for o, a in opts:
		if o == '-s':
			if mode:
				_help()
			mode = 'status'
		if o == '-l':
			if mode:
				_help()
			mode = 'list'
		if o == '-L':
			if mode:
				_help()
			mode = 'longlist'
		if o == '-i':
			mode = 'install'
		if o == '-f':
			force = 1
		if o == '-v':
			verbose = 1
	if not mode:
		_help()
	_run(mode, verbose, force, args)
				
if __name__ == '__main__':
	main()
	
	