#
# Genmodule - A python program to help you build (template) modules.
#
# Usage:
#
# o = genmodule.object()
# o.name = 'dwarve object'
# o.abbrev = 'dw'
# o.funclist = ['new', 'dealloc', 'getattr', 'setattr']
# o.methodlist = ['dig']
#
# m = genmodule.module()
# m.name = 'beings'
# m.abbrev = 'be'
# m.methodlist = ['newdwarve']
# m.objects = [o]
#
# genmodule.write(sys.stdout, m)
#
import sys
import os
import varsubst

error = 'genmodule.error'

#
# Names of functions in the object-description struct.
#
FUNCLIST = ['tp_dealloc', 'tp_init', 'tp_new',
            'tp_print', 'tp_getattr', 'tp_setattr',
            'tp_compare', 'tp_repr', 'tp_hash', 'tp_call', 'tp_str',
            'tp_descr_get', 'tp_descr_set',
            'tp_iter', 'tp_iternext'
            ]
TYPELIST = ['tp_as_number', 'tp_as_sequence', 'tp_as_mapping',
            'structure', 'getset', 'gc']

#
# writer is a base class for the object and module classes
# it contains code common to both.
#
class writer:
    def __init__(self):
        self._subst = None

    def makesubst(self):
        if not self._subst:
            if not self.__dict__.has_key('abbrev'):
                self.abbrev = self.name
            self.Abbrev = self.abbrev[0].upper()+self.abbrev[1:]
            subst = varsubst.Varsubst(self.__dict__)
            subst.useindent(1)
            self._subst = subst.subst

    def addcode(self, name, fp):
        ifp = self.opentemplate(name)
        self.makesubst()
        d = ifp.read()
        d = self._subst(d)
        fp.write(d)

    def opentemplate(self, name):
        for p in sys.path:
            fn = os.path.join(p, name)
            if os.path.exists(fn):
                return open(fn, 'r')
            fn = os.path.join(p, 'Templates')
            fn = os.path.join(fn, name)
            if os.path.exists(fn):
                return open(fn, 'r')
        raise error, 'Template '+name+' not found for '+self._type+' '+ \
                     self.name
        
class module(writer):
    _type = 'module'

    def writecode(self, fp):
        self.addcode('copyright', fp)
        self.addcode('module_head', fp)
        for o in self.objects:
            o.module = self.name
        for o in self.objects:
            o.writehead(fp)
        for o in self.objects:
            o.writebody(fp)
        new_ml = ''
        for fn in self.methodlist:
            self.method = fn
            self.addcode('module_method', fp)
            new_ml = new_ml + (
                      '\t{"%s",\t(PyCFunction)%s_%s,\tMETH_VARARGS,\n'
                      '\t %s_%s__doc__},\n'
                      %(fn, self.abbrev, fn, self.abbrev, fn))
        self.methodlist = new_ml

        if self.objects:
            ready_types = ['', '/* Initialize types: */']
            add_types = ['', '/* Add types: */']

            for o in self.objects:
                ready_types.extend(['if (PyType_Ready(&%s%s) < 0)'
                                    % (o.abbrev, o.type),
                                    '\treturn;'
                                    ])
                add_types.extend(['if (PyModule_AddObject(m, "%s", '
                                  '(PyObject *)&%s%s) < 0)'
                                  % (o.name, o.abbrev, o.type),
                                  '\treturn;'
                                  ])
            self.ready_types = '\n'.join(ready_types)+'\n'
            self.add_types = '\n'.join(add_types)+'\n'
            
        else:
            self.ready_types = self.add_types = ''
        
        self.addcode('module_tail', fp)

class object(writer):
    _type = 'object'

    
    def __init__(self):
        self.typelist = []
        self.methodlist = []
        self.funclist = []
        self.object = ''
        self.type = 'Type'
        writer.__init__(self)

    def writecode(self, fp):
        self.addcode('copyright', fp)
        self.writehead(fp)
        self.writebody(fp)

    def writehead(self, fp):
        self.addcode('object_head', fp)

    def writebody(self, fp):
        if self.methodlist:
            new_ml = ''
            for fn in self.methodlist:
                self.method = fn
                self.addcode('object_method', fp)
                new_ml = new_ml + (
                          '\t{"%s",\t(PyCFunction)%s_%s,\tMETH_VARARGS,\n'
                          '\t %s_%s__doc__},\n'
                          %(fn, self.abbrev, fn, self.abbrev, fn))
            self.methodlist = new_ml
            self.addcode('object_mlist', fp)

            self.tp_methods = self.abbrev + '_methods'

        else:
            self.tp_methods = '0'

        if 'gc' in self.typelist:
            self.call_clear = '\n\t' + self.abbrev + '_clear(self);'
            if 'tp_dealloc' in self.funclist:
                self.funclist.remove('tp_dealloc')
            self.typelist.append('tp_dealloc')
            self.extra_flags = "\n\t| Py_TPFLAGS_HAVE_GC"
            self.tp_traverse =  self.abbrev + "_traverse"
            self.tp_clear = self.abbrev + "_clear"
        else:
            self.extra_flags = ""
            self.tp_traverse = self.tp_clear = "0"
            self.call_clear = ''
            
            
        for fn in FUNCLIST:
            setattr(self, fn, '0')

        if 'structure' in self.typelist:
            self.tp_members = self.abbrev + '_members'
        else:
            self.tp_members = '0'

        if 'getset' in self.typelist:
            self.tp_getset = self.abbrev + '_getset'
        else:
            self.tp_getset = '0'

        if 'tp_new' not in self.funclist:
            self.tp_new = 'PyType_GenericNew'
        
        for fn in self.funclist:
            self.addcode('object_'+fn, fp)
            setattr(self, fn, '%s_%s'%(self.abbrev, fn[3:]))
        for tn in TYPELIST:
            setattr(self, tn, '0')
        for tn in self.typelist:
            self.addcode('object_'+tn, fp)
            setattr(self, tn, '&%s_%s'%(self.abbrev, tn[3:]))
        self.addcode('object_tail', fp)

def write(fp, obj):
    obj.writecode(fp)

if __name__ == '__main__':
    o = object()
    o.name = 'dwarve object'
    o.abbrev = 'dw'
    o.funclist = ['new', 'tp_dealloc']
    o.methodlist = ['dig']
    m = module()
    m.name = 'beings'
    m.abbrev = 'be'
    m.methodlist = ['newdwarve']
    m.objects = [o]
    write(sys.stdout, m)
