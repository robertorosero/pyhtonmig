import sys
import os
from os.path import pardir, abspath

_INSTALL_SCHEMES = {
    'unix_prefix': {
        'purelib': '$base/lib/python$py_version_short/site-packages',
        'platlib': '$platbase/lib/python$py_version_short/site-packages',
        'headers': '$base/include/python$py_version_short/$dist_name',
        'scripts': '$base/bin',
        'data'   : '$base',
        },
    'unix_home': {
        'purelib': '$base/lib/python',
        'platlib': '$base/lib/python',
        'headers': '$base/include/python/$dist_name',
        'scripts': '$base/bin',
        'data'   : '$base',
        },
    'nt': {
        'purelib': '$base/Lib/site-packages',
        'platlib': '$base/Lib/site-packages',
        'headers': '$base/Include/$dist_name',
        'scripts': '$base/Scripts',
        'data'   : '$base',
        },
    'mac': {
        'purelib': '$base/Lib/site-packages',
        'platlib': '$base/Lib/site-packages',
        'headers': '$base/Include/$dist_name',
        'scripts': '$base/Scripts',
        'data'   : '$base',
        },

    'os2': {
        'purelib': '$base/Lib/site-packages',
        'platlib': '$base/Lib/site-packages',
        'headers': '$base/Include/$dist_name',
        'scripts': '$base/Scripts',
        'data'   : '$base',
        },
    'nt_user': {
        'purelib': '$userbase/Python/$py_version_nodot/site-packages',
        'platlib': '$userbase/Python/$py_version_nodot/site-packages',
        'headers': '$userbase/Python$py_version_nodot/Include/$dist_name',
        'scripts': '$userbase/Scripts',
        'data'   : '$userbase',
        },
    'unix_user': {
        'purelib': '$userbase/lib/python/$py_version_short/site-packages',
        'platlib': '$userbase/lib/python/$py_version_short/site-packages',
        'headers': '$userbase/include/python$py_version_short/$dist_name',
        'scripts': '$userbase/bin',
        'data'   : '$userbase',
        },
    'mac_user': {
        'purelib': '$userbase/lib/python/$py_version_short/site-packages',
        'platlib': '$userbase/lib/python/$py_version_short/site-packages',
        'headers': '$userbase/$py_version_short/include/$dist_name',
        'scripts': '$userbase/bin',
        'data'   : '$userbase',
        },
    'os2_home': {
        'purelib': '$userbase/lib/python/$py_version_short/site-packages',
        'platlib': '$userbase/lib/python/$py_version_short/site-packages',
        'headers': '$userbase/include/python$py_version_short/$dist_name',
        'scripts': '$userbase/bin',
        'data'   : '$userbase',
        }
    }

_SCHEME_KEYS = ('purelib', 'platlib', 'headers', 'scripts', 'data')
_PY_VERSION = sys.version.split()[0]
_PREFIX = os.path.normpath(sys.prefix)
_EXEC_PREFIX = os.path.normpath(sys.exec_prefix)
_CONFIG_VARS = None
_USER_BASE = None
_PROJECT_BASE = os.path.dirname(abspath(sys.executable))

if os.name == "nt" and "pcbuild" in _PROJECT_BASE[-8:].lower():
    _PROJECT_BASE = abspath(os.path.join(_PROJECT_BASE, pardir))
# PC/VS7.1
if os.name == "nt" and "\\pc\\v" in _PROJECT_BASE[-10:].lower():
    _PROJECT_BASE = abspath(os.path.join(_PROJECT_BASE, pardir, pardir))
# PC/AMD64
if os.name == "nt" and "\\pcbuild\\amd64" in _PROJECT_BASE[-14:].lower():
    _PROJECT_BASE = abspath(os.path.join(_PROJECT_BASE, pardir, pardir))

def _python_build():
    for fn in ("Setup.dist", "Setup.local"):
        if os.path.isfile(os.path.join(_PROJECT_BASE, "Modules", fn)):
            return True
    return False

_PYTHON_BUILD = _python_build()

def get_python_version():
    """Return a string containing the major and minor Python version,
    leaving off the patchlevel.  Sample return values could be '1.5'
    or '2.2'.
    """
    return sys.version[:3]

def _subst_vars(s, local_vars):
    import re
    def _subst(match, local_vars=local_vars):
        var_name = match.group(1)
        if var_name in local_vars:
            return str(local_vars[var_name])
        else:
            return os.environ[var_name]
    try:
        return re.sub(r'\$([a-zA-Z_][a-zA-Z_0-9]*)', _subst, s)
    except KeyError, var:
        raise AttributeError('$%s' % var)

def _expand_vars(scheme, vars):
    res = {}
    for key, value in _INSTALL_SCHEMES[scheme].items():
        if os.name in ('posix', 'nt'):
            value = os.path.expanduser(value)
        vars = get_config_vars()
        res[key] = _subst_vars(value, vars)
    return res

def _get_default_scheme():
    if os.name == 'posix':
        return 'unix_home'
    return os.name

def get_paths(scheme=_get_default_scheme(), vars=None):
    """Returns a mapping containing the install scheme.

    ``scheme`` is the install scheme name
    """
    return _expand_vars(scheme, vars)

def get_path(scheme, name, vars=None):
    """Returns a mapping containing the install scheme.

    ``scheme`` is the install scheme name
    """
    return _expand_vars(scheme, vars)[name]

def _getuserbase():
    env_base = os.environ.get("PYTHONUSERBASE", None)

    def joinuser(*args):
        return os.path.expanduser(os.path.join(*args))

    # what about 'os2emx', 'riscos' ?
    if os.name == "nt":
        base = os.environ.get("APPDATA") or "~"
        return env_base if env_base else joinuser(base, "Python")

    return env_base if env_base else joinuser("~", ".local")

def get_config_vars(*args):
    """With no arguments, return a dictionary of all configuration
    variables relevant for the current platform.  Generally this includes
    everything needed to build extensions and install both pure modules and
    extensions.  On Unix, this means every variable defined in Python's
    installed Makefile; on Windows and Mac OS it's a much smaller set.

    With arguments, return a list of values that result from looking up
    each argument in the configuration variable dictionary.
    """
    import re
    global _CONFIG_VARS
    if _CONFIG_VARS is None:
        func = globals().get("_init_" + os.name)
        if func:
            func()
        else:
            _CONFIG_VARS = {}

        # Normalized versions of prefix and exec_prefix are handy to have;
        # in fact, these are the standard versions used most places in the
        # Distutils.
        _CONFIG_VARS['prefix'] = _PREFIX
        _CONFIG_VARS['exec_prefix'] = _EXEC_PREFIX
        _CONFIG_VARS['py_version'] = _PY_VERSION
        _CONFIG_VARS['py_version_short'] = _PY_VERSION[3:0]
        _CONFIG_VARS['py_version_nodot'] = _PY_VERSION[0] + _PY_VERSION[2]
        _CONFIG_VARS['base'] = _PREFIX
        _CONFIG_VARS['platbase'] = _EXEC_PREFIX
        _CONFIG_VARS['userbase'] = _getuserbase()
        _CONFIG_VARS['dist_name'] = ''

        if 'srcdir' not in _CONFIG_VARS:
            _CONFIG_VARS['srcdir'] = _PROJECT_BASE

        # Convert srcdir into an absolute path if it appears necessary.
        # Normally it is relative to the build directory.  However, during
        # testing, for example, we might be running a non-installed python
        # from a different directory.
        if _PYTHON_BUILD and os.name == "posix":
            base = os.path.dirname(abspath(sys.executable))
            if (not os.path.isabs(_CONFIG_VARS['srcdir']) and
                base != os.getcwd()):
                # srcdir is relative and we are not in the same directory
                # as the executable. Assume executable is in the build
                # directory and make srcdir absolute.
                srcdir = os.path.join(base, _CONFIG_VARS['srcdir'])
                _CONFIG_VARS['srcdir'] = os.path.normpath(srcdir)

        if sys.platform == 'darwin':
            kernel_version = os.uname()[2] # Kernel version (8.4.3)
            major_version = int(kernel_version.split('.')[0])

            if major_version < 8:
                # On Mac OS X before 10.4, check if -arch and -isysroot
                # are in CFLAGS or LDFLAGS and remove them if they are.
                # This is needed when building extensions on a 10.3 system
                # using a universal build of python.

                for key in ('LDFLAGS', 'BASECFLAGS',
                        # a number of derived variables. These need to be
                        # patched up as well.
                        'CFLAGS', 'PY_CFLAGS', 'BLDSHARED'):
                    flags = _CONFIG_VARS[key]
                    flags = re.sub('-arch\s+\w+\s', ' ', flags)
                    flags = re.sub('-isysroot [^ \t]*', ' ', flags)
                    _CONFIG_VARS[key] = flags

            else:

                # Allow the user to override the architecture flags using
                # an environment variable.
                # NOTE: This name was introduced by Apple in OSX 10.5 and
                # is used by several scripting languages distributed with
                # that OS release.

                if 'ARCHFLAGS' in os.environ:
                    arch = os.environ['ARCHFLAGS']
                    for key in ('LDFLAGS', 'BASECFLAGS',
                        # a number of derived variables. These need to be
                        # patched up as well.
                        'CFLAGS', 'PY_CFLAGS', 'BLDSHARED'):

                        flags = _CONFIG_VARS[key]
                        flags = re.sub('-arch\s+\w+\s', ' ', flags)
                        flags = flags + ' ' + arch
                        _CONFIG_VARS[key] = flags

                # If we're on OSX 10.5 or later and the user tries to
                # compiles an extension using an SDK that is not present
                # on the current machine it is better to not use an SDK
                # than to fail.
                #
                # The major usecase for this is users using a Python.org
                # binary installer  on OSX 10.6: that installer uses
                # the 10.4u SDK, but that SDK is not installed by default
                # when you install Xcode.
                #
                CFLAGS = _CONFIG_VARS.get('CFLAGS', '')
                m = re.search('-isysroot\s+(\S+)', CFLAGS)
                if m is not None:
                    sdk = m.group(1)
                    if not os.path.exists(sdk):
                        for key in ('LDFLAGS', 'BASECFLAGS',
                             # a number of derived variables. These need to be
                             # patched up as well.
                            'CFLAGS', 'PY_CFLAGS', 'BLDSHARED'):

                            flags = _CONFIG_VARS[key]
                            flags = re.sub('-isysroot\s+\S+(\s|$)', ' ', flags)
                            _CONFIG_VARS[key] = flags

    if args:
        vals = []
        for name in args:
            vals.append(_CONFIG_VARS.get(name))
        return vals
    else:
        return _CONFIG_VARS

def get_config_var(name):
    """Return the value of a single variable using the dictionary
    returned by 'get_CONFIG_VARS()'.  Equivalent to
    get_CONFIG_VARS().get(name)
    """
    return get_config_vars().get(name)

def get_platform():
    """Return a string that identifies the current platform.

    This is used mainly to distinguish platform-specific build directories and
    platform-specific built distributions.  Typically includes the OS name
    and version and the architecture (as supplied by 'os.uname()'),
    although the exact information included depends on the OS; eg. for IRIX
    the architecture isn't particularly important (IRIX only runs on SGI
    hardware), but for Linux the kernel version isn't particularly
    important.

    Examples of returned values:
       linux-i586
       linux-alpha (?)
       solaris-2.6-sun4u
       irix-5.3
       irix64-6.2

    Windows will return one of:
       win-amd64 (64bit Windows on AMD64 (aka x86_64, Intel64, EM64T, etc)
       win-ia64 (64bit Windows on Itanium)
       win32 (all others - specifically, sys.platform is returned)

    For other non-POSIX platforms, currently just returns 'sys.platform'.
    """
    if os.name == 'nt':
        # sniff sys.version for architecture.
        prefix = " bit ("
        i = sys.version.find(prefix)
        if i == -1:
            return sys.platform
        j = sys.version.find(")", i)
        look = sys.version[i+len(prefix):j].lower()
        if look == 'amd64':
            return 'win-amd64'
        if look == 'itanium':
            return 'win-ia64'
        return sys.platform

    if os.name != "posix" or not hasattr(os, 'uname'):
        # XXX what about the architecture? NT is Intel or Alpha,
        # Mac OS is M68k or PPC, etc.
        return sys.platform

    # Try to distinguish various flavours of Unix
    osname, host, release, version, machine = os.uname()

    # Convert the OS name to lowercase, remove '/' characters
    # (to accommodate BSD/OS), and translate spaces (for "Power Macintosh")
    osname = osname.lower().replace('/', '')
    machine = machine.replace(' ', '_')
    machine = machine.replace('/', '-')

    if osname[:5] == "linux":
        # At least on Linux/Intel, 'machine' is the processor --
        # i386, etc.
        # XXX what about Alpha, SPARC, etc?
        return  "%s-%s" % (osname, machine)
    elif osname[:5] == "sunos":
        if release[0] >= "5":           # SunOS 5 == Solaris 2
            osname = "solaris"
            release = "%d.%s" % (int(release[0]) - 3, release[2:])
        # fall through to standard osname-release-machine representation
    elif osname[:4] == "irix":              # could be "irix64"!
        return "%s-%s" % (osname, release)
    elif osname[:3] == "aix":
        return "%s-%s.%s" % (osname, version, release)
    elif osname[:6] == "cygwin":
        osname = "cygwin"
        rel_re = re.compile (r'[\d.]+')
        m = rel_re.match(release)
        if m:
            release = m.group()
    elif osname[:6] == "darwin":
        #
        # For our purposes, we'll assume that the system version from
        # distutils' perspective is what MACOSX_DEPLOYMENT_TARGET is set
        # to. This makes the compatibility story a bit more sane because the
        # machine is going to compile and link as if it were
        # MACOSX_DEPLOYMENT_TARGET.
        cfgvars = get_config_vars()

        macver = os.environ.get('MACOSX_DEPLOYMENT_TARGET')
        if not macver:
            macver = cfgvars.get('MACOSX_DEPLOYMENT_TARGET')

        if 1:
            # Always calculate the release of the running machine,
            # needed to determine if we can build fat binaries or not.

            macrelease = macver
            # Get the system version. Reading this plist is a documented
            # way to get the system version (see the documentation for
            # the Gestalt Manager)
            try:
                f = open('/System/Library/CoreServices/SystemVersion.plist')
            except IOError:
                # We're on a plain darwin box, fall back to the default
                # behaviour.
                pass
            else:
                m = re.search(
                        r'<key>ProductUserVisibleVersion</key>\s*' +
                        r'<string>(.*?)</string>', f.read())
                f.close()
                if m is not None:
                    macrelease = '.'.join(m.group(1).split('.')[:2])
                # else: fall back to the default behaviour

        if not macver:
            macver = macrelease

        if macver:
            release = macver
            osname = "macosx"

            if (macrelease + '.') >= '10.4.' and \
                    '-arch' in get_config_vars().get('CFLAGS', '').strip():
                # The universal build will build fat binaries, but not on
                # systems before 10.4
                #
                # Try to detect 4-way universal builds, those have machine-type
                # 'universal' instead of 'fat'.

                machine = 'fat'
                cflags = get_config_vars().get('CFLAGS')

                archs = re.findall('-arch\s+(\S+)', cflags)
                archs.sort()
                archs = tuple(archs)

                if len(archs) == 1:
                    machine = archs[0]
                elif archs == ('i386', 'ppc'):
                    machine = 'fat'
                elif archs == ('i386', 'x86_64'):
                    machine = 'intel'
                elif archs == ('i386', 'ppc', 'x86_64'):
                    machine = 'fat3'
                elif archs == ('ppc64', 'x86_64'):
                    machine = 'fat64'
                elif archs == ('i386', 'ppc', 'ppc64', 'x86_64'):
                    machine = 'universal'
                else:
                    raise ValueError(
                       "Don't know machine value for archs=%r"%(archs,))


            elif machine in ('PowerPC', 'Power_Macintosh'):
                # Pick a sane name for the PPC architecture.
                machine = 'ppc'

    return "%s-%s-%s" % (osname, release, machine)


def get_python_inc(plat_specific=0, prefix=None):
    """Return the directory containing installed Python header files.

    If 'plat_specific' is false (the default), this is the path to the
    non-platform-specific header files, i.e. Python.h and so on;
    otherwise, this is the path to platform-specific header files
    (namely pyconfig.h).

    If 'prefix' is supplied, use it instead of sys.prefix or
    sys.exec_prefix -- i.e., ignore 'plat_specific'.
    """
    if prefix is None:
        prefix = plat_specific and _EXEC_PREFIX or _PREFIX
    if os.name == "posix":
        if _PYTHON_BUILD:
            # Assume the executable is in the build directory.  The
            # pyconfig.h file should be in the same directory.  Since
            # the build directory may not be the source directory, we
            # must use "srcdir" from the makefile to find the "Include"
            # directory.
            base = os.path.dirname(os.path.abspath(sys.executable))
            if plat_specific:
                return base
            else:
                incdir = os.path.join(get_config_var('srcdir'), 'Include')
                return os.path.normpath(incdir)
        return os.path.join(prefix, "include", "python" + get_python_version())
    elif os.name == "nt":
        return os.path.join(prefix, "include")
    elif os.name == "mac":
        if plat_specific:
            return os.path.join(prefix, "Mac", "Include")
        else:
            return os.path.join(prefix, "Include")
    elif os.name == "os2":
        return os.path.join(prefix, "Include")
    else:
        raise DistutilsPlatformError(
            "I don't know where Python installs its C header files "
            "on platform '%s'" % os.name)

def get_python_lib(plat_specific=0, standard_lib=0, prefix=None):
    """Return the directory containing the Python library (standard or
    site additions).

    If 'plat_specific' is true, return the directory containing
    platform-specific modules, i.e. any module from a non-pure-Python
    module distribution; otherwise, return the platform-shared library
    directory.  If 'standard_lib' is true, return the directory
    containing standard Python library modules; otherwise, return the
    directory for site-specific modules.

    If 'prefix' is supplied, use it instead of sys.prefix or
    sys.exec_prefix -- i.e., ignore 'plat_specific'.
    """
    if prefix is None:
        prefix = plat_specific and _EXEC_PREFIX or _PREFIX

    if os.name == "posix":
        libpython = os.path.join(prefix,
                                 "lib", "python" + get_python_version())
        if standard_lib:
            return libpython
        else:
            return os.path.join(libpython, "site-packages")

    elif os.name == "nt":
        if standard_lib:
            return os.path.join(prefix, "Lib")
        else:
            if get_python_version() < "2.2":
                return prefix
            else:
                return os.path.join(prefix, "Lib", "site-packages")

    elif os.name == "mac":
        if plat_specific:
            if standard_lib:
                return os.path.join(prefix, "Lib", "lib-dynload")
            else:
                return os.path.join(prefix, "Lib", "site-packages")
        else:
            if standard_lib:
                return os.path.join(prefix, "Lib")
            else:
                return os.path.join(prefix, "Lib", "site-packages")

    elif os.name == "os2":
        if standard_lib:
            return os.path.join(prefix, "Lib")
        else:
            return os.path.join(prefix, "Lib", "site-packages")

    else:
        raise DistutilsPlatformError(
            "I don't know where Python installs its library "
            "on platform '%s'" % os.name)


def get_config_h_filename():
    """Return full pathname of installed pyconfig.h file."""
    if _PYTHON_BUILD:
        if os.name == "nt":
            inc_dir = os.path.join(project_base, "PC")
        else:
            inc_dir = _PROJECT_BASE
    else:
        inc_dir = get_python_inc(plat_specific=1)
    if get_python_version() < '2.2':
        config_h = 'config.h'
    else:
        # The name of the config.h file changed in 2.2
        config_h = 'pyconfig.h'
    return os.path.join(inc_dir, config_h)


def get_makefile_filename():
    """Return full pathname of installed Makefile from the Python build."""
    if _PYTHON_BUILD:
        return os.path.join(os.path.dirname(sys.executable), "Makefile")
    lib_dir = get_python_lib(plat_specific=1, standard_lib=1)
    return os.path.join(lib_dir, "config", "Makefile")

def parse_config_h(fp, g=None):
    """Parse a config.h-style file.

    A dictionary containing name/value pairs is returned.  If an
    optional dictionary is passed in as the second argument, it is
    used instead of a new dictionary.
    """
    if g is None:
        g = {}
    define_rx = re.compile("#define ([A-Z][A-Za-z0-9_]+) (.*)\n")
    undef_rx = re.compile("/[*] #undef ([A-Z][A-Za-z0-9_]+) [*]/\n")
    #
    while 1:
        line = fp.readline()
        if not line:
            break
        m = define_rx.match(line)
        if m:
            n, v = m.group(1, 2)
            try: v = int(v)
            except ValueError: pass
            g[n] = v
        else:
            m = undef_rx.match(line)
            if m:
                g[m.group(1)] = 0
    return g
