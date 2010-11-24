import sys
import ast
from pprint import pprint
#print("__optimizer__.py imported!")

# Taken from symtable.h:
DEF_GLOBAL     = 1      # global stmt
DEF_LOCAL      = 2      # assignment in code block
DEF_PARAM      = 2<<1   # formal parameter
DEF_NONLOCAL   = 2<<2   # nonlocal stmt
USE            = 2<<3   # name is used
DEF_FREE       = 2<<4   # name used but not defined in nested block
DEF_FREE_CLASS = 2<<5   # free variable from class's method
DEF_IMPORT     = 2<<6   # assignment occurred via import

DEF_BOUND = (DEF_LOCAL | DEF_PARAM | DEF_IMPORT)

#  GLOBAL_EXPLICIT and GLOBAL_IMPLICIT are used internally by the symbol
#  table.  GLOBAL is returned from PyST_GetScope() for either of them.
#   It is stored in ste_symbols at bits 12-15.

SCOPE_OFFSET = 11
SCOPE_MASK  = (DEF_GLOBAL | DEF_LOCAL | DEF_PARAM | DEF_NONLOCAL)

LOCAL = 1
GLOBAL_EXPLICIT = 2
GLOBAL_IMPLICIT = 3
FREE = 4
CELL = 5


# Suppress various local-manipulation transforms in the presence of usage
# of these builtins:
BUILTINS_THAT_READ_LOCALS = {'locals', 'vars', 'dir'}

def log(msg):
    if 0:
        print(msg)

# Analogous to PyST_GetScope:
def get_scope(ste, name):
    v = ste.symbols.get(name, 0)
    if v:
        return (v >> SCOPE_OFFSET) & SCOPE_MASK
    else:
        return 0

def add_local(ste, name):
    # Add a new local var to an STE
    assert name not in ste.varnames
    assert name not in ste.symbols
    ste.symbols[name] = (LOCAL << SCOPE_OFFSET)
    ste.varnames.append(name)

def to_dot(t):
    def _node_to_dot(node, indent):
        result = ''
        prefix = '  ' * indent
        if isinstance(node, ast.AST):
            if hasattr(node, 'ste'):
                result += prefix + 'subgraph cluster_%s {\n' % id(node)
                result += prefix + '  label = "%s"\n;' % node.ste.name
            result += prefix + '  node%i [label=<%s>];\n' % (id(node), node.__class__.__name__)
            for name, field in ast.iter_fields(node):
                if field is not None:
                    result += prefix + '  node%i -> node%i [label="%s"];\n' % (id(node), id(field), name)
                    result += _node_to_dot(field, indent + 2)
            if hasattr(node, 'ste'):
                result += prefix + '}\n'
        elif isinstance(node, list):
            result += prefix + 'node%i [label=<[]>];\n' % (id(node))
            for i, item in enumerate(node):
                result += prefix + 'node%i -> node%i [label="[%i]"];\n' % (id(node), id(item), i)
                result += _node_to_dot(item, indent)
        elif node is None:
            pass
        else:
            result += prefix + 'node%i [label=<%s>];\n' % (id(node), repr(node))
        return result

    result = 'digraph {\n'
    result += _node_to_dot(t, 1)
    result += '}'
    return result

def dot_to_png(dot, filename):
    from subprocess import Popen, PIPE
    p = Popen(['/usr/bin/dot',
               '-T', 'png',
               '-o', filename],
              stdin=PIPE)
    p.communicate(dot.encode('utf-8'))
    p.wait()

class NodePathEntry:
    __slots__ = ('node',  # the ast.Node
                 'field', # the name of the field
                 'index', # the index within the field (for lists), or None
                 )
    def __init__(self, node, field, index):
        self.node = node
        self.field = field
        self.index = index

    def __str__(self):
        result = self.node.__class__.__name__
        if hasattr(self.node, 'ste'):
            result += '<%s>' % repr(self.node.ste.name)
        result += '.%s' % self.field
        if self.index is not None:
            result += '[%i]' % self.index
        return result

class NodePath:
    '''
    A list of NodePathEntries
    '''
    __slots__ = ('entries', )

    def __init__(self, entries):
        self.entries = entries

    def __str__(self):
        return '/'.join([str(entry) for entry in self.entries])

    def __repr__(self):
        return '/'.join([str(entry) for entry in self.entries])

    def extend(self, node, field, index):
        return NodePath(self.entries + [NodePathEntry(node, field, index)])

    def get_dotted_name(self, childnode=None):
        nsp = NamespacePath.from_node_path(self, childnode)
        return nsp.as_dotted_str()

class NamespacePath:
    '''
    A list of symbol table entries
    '''
    __slots__ = ('_stes',)

    def __init__(self, stes):
        self._stes = stes

    @classmethod
    def from_node_path(cls, nodepath, childnode=None):
        result = []
        for npe in nodepath.entries:
            if hasattr(npe.node, 'ste'):
                result.append(npe.node.ste)
        if childnode is not None:
            if hasattr(childnode, 'ste'):
                result.append(childnode.ste)
        return NamespacePath(result)

    def as_dotted_str(self):
        '''
        Generate a dotted string representing the namespace e.g. "SomeClass.some_method"
        '''
        # Start at 1: don't include the "top" STE:
        return '.'.join([ste.name for ste in self._stes[1:]])

    def get_parent_path(self):
        return NamespacePath(self._stes[:-1])

    def get_innermost_scope(self):
        return self._stes[-1]



class PathTransformer:
    """
    Similar to an ast.NodeTransformer, but passes in a path when visiting a node

    The path is passed in as a list of (node, field, index) triples
    """
    def visit(self, node, path=None):
        """Visit a node."""
        method = 'visit_' + node.__class__.__name__
        visitor = getattr(self, method, self.generic_visit)
        if path is None:
            path = NodePath([])
        return visitor(node, path)

    def generic_visit(self, node, path):
        for field, old_value in ast.iter_fields(node):
            old_value = getattr(node, field, None)
            if isinstance(old_value, list):
                new_values = []
                for idx, value in enumerate(old_value):
                    if isinstance(value, ast.AST):
                        value = self.visit(value, path.extend(node, field, idx))
                        if value is None:
                            continue
                        elif not isinstance(value, ast.AST):
                            new_values.extend(value)
                            continue
                    new_values.append(value)
                old_value[:] = new_values
            elif isinstance(old_value, ast.AST):
                new_node = self.visit(old_value, path.extend(node, field, None))
                if new_node is None:
                    delattr(node, field)
                else:
                    setattr(node, field, new_node)
        return node

def make_load_name(name, locnode):
    return ast.copy_location(ast.Name(id=name, ctx=ast.Load()), locnode)

def make_assignment(name, expr, locnode):
    name_node = ast.copy_location(ast.Name(id=name, ctx=ast.Store()), locnode)
    return ast.copy_location(ast.Assign(targets=[name_node],
                                        value=expr),
                             locnode)

def ast_clone(node):
    #log('ast_clone', node)
    if isinstance(node, ast.AST):
        clone = node.__class__()
        clone = ast.copy_location(clone, node)
        for name, value in ast.iter_fields(node):
            if isinstance(value, ast.AST):
                cvalue = ast_clone(value)
            elif isinstance(value, list):
                cvalue = [ast_clone(el) for el in value]
            else:
                cvalue = value
            setattr(clone, name, cvalue)
        return clone
    elif isinstance(node, str):
        return node
    else:
        raise ValueError("Don't know how to clone %r" % node)

class InlineBodyFixups(ast.NodeTransformer):
    """
    Fix up the cloned body of a function, for inlining
    """
    def __init__(self, varprefix, ste):
        self.varprefix = varprefix
        self.ste = ste

    def visit_Name(self, node):
        # Replace local names with prefixed versions:
        self.generic_visit(node)
        scope = get_scope(self.ste, node.id)
        if scope == LOCAL:
            node.id = self.varprefix + node.id
        return node

    def visit_Return(self, node):
        self.generic_visit(node)
        # replace (the final) return with "__returnval__ = expr":
        return make_assignment(self.varprefix + "__returnval__", node.value, node)


class FunctionInliner(PathTransformer):
    def __init__(self, tree, def_dict):
        self.tree = tree
        self.def_dict = def_dict
        # dict from dottedname to ast.FunctionDef

        #self.funcdef = funcdef
        #self.dotted_name = dotted_name
        #assert hasattr(funcdef, 'ste')

        self.num_callsites = 0
        self.log('inlining calls to %r' % def_dict)
        #self.log('ste for body: %r' % funcdef.ste)

    def log(self, msg):
        if 0:
            print('%s: %s' % (self.__class__.__name__, msg))

    def guess_dotted_name_for_def_from_call(self, call, path):
        # Return the name of the stored global if this is inlinable, otherwise None
        assert isinstance(call, ast.Call)

        if isinstance(call.func, ast.Name):
            # Name must match:
            if call.func.id in self.def_dict:
                return call.func.id

        if isinstance(call.func, ast.Attribute):
            # Handle simple "self.METHOD_NAME" case:
            attr = call.func
            value = attr.value
            if isinstance(value, ast.Name) and isinstance(value.ctx, ast.Load):
                if value.id == 'self':
                    #print('attr.attr:', attr.attr)
                    #print('path:', path)
                    #print(path.get_dotted_name())
                    parent_nsp = NamespacePath.from_node_path(path).get_parent_path()
                    return parent_nsp.as_dotted_str() + '.' + attr.attr
# FIXME: only makes sense to traverse within this class and within subclasses
# FIXME: fake the MRO and pick an appropriate class

        # Don't try to inline where the function is a non-trivial
        # expression e.g. "f()()", or for other awkward cases
        return None

    def _setup_args(self, ste, varprefix, funcdef, call, instance):
        # Create assignment statements of the form:
        #    __inline__x = expr for x
        # for each parameter
        # We will insert before the callsite

        assignments = []

        formalparams = funcdef.args.args
        actualparams = call.args
        if instance:
            # Synthesize a "self" at the front of the params:
            # Note that this must be just a load of a local name, to avoid,
            # say, injecting a 2nd getattr:
            assert is_read_from_local(ste, instance)
            actualparams = [instance] + actualparams

        for formal, actual in zip(formalparams, actualparams):
            self.log('  formal: %s' % ast.dump(formal))
            self.log('    actual: %s' % ast.dump(actual))
            add_local(ste, varprefix+formal.arg)

            assign = make_assignment(varprefix+formal.arg, actual, call)
            assignments.append(assign)

            # FIXME: these seem to be being done using LOAD_NAME/STORE_NAME; why isn't it using _FAST?
            # aha: because they're in module scope, not within a function.

        return assignments

    def visit_Call(self, call, path):
        # Stop inlining beyond an arbitrary cutoff
        # (bm_simplecall was exploding):
        if self.num_callsites > 1000:
            return call

        # Visit children:
        self.generic_visit(call, path)

        dotted_name = self.guess_dotted_name_for_def_from_call(call, path)
        if dotted_name is None:
            return call

        self.log('Got inlinable callsite of:\n  dotted_name: %r\n  path: %s\n  node:%s'
                 % (dotted_name, path, ast.dump(call)))

        if not isinstance(call.func, (ast.Name, ast.Attribute)):
            # Don't try to inline where the function is a non-trivial
            # expression e.g. "f()()"
            print('foo!')
            return call

        if isinstance(call.func, ast.Attribute):
            # Emergency cutoff for method inlining:
            if 0:
                self.log('Not inlining attribute %s' % ast.dump(call.func))
                return call

        self.log('Considering call to: %s' % ast.dump(call.func))
        self.log('NodePath: %r' % path)
        nsp = NamespacePath.from_node_path(path)
        self.log('NamespacePath for callsite: %r' % nsp)

        if dotted_name not in self.def_dict:
            return call
        #if call.func.id != self.funcdef.name:
        #    return call

        # Locate innermost scope at callsite:
        ste = nsp.get_innermost_scope()

        self.log('Inlining call to: %r within %r' % (dotted_name, ste.name))
        self.num_callsites += 1

        funcdef = self.def_dict[dotted_name]

        self.log(ast.dump(funcdef))
        varprefix = '__internal__inline_%s%x__' % (dotted_name, id(call))
        self.log('varprefix: %s' % varprefix)

        # Generate a body of specialized statements that can replace the call:
        if isinstance(call.func, ast.Attribute):
            attr = call.func
            value = attr.value
            assert isinstance(value, ast.Name) and isinstance(value.ctx, ast.Load)
            instance = value
        else:
            instance = None
        specialized = self._setup_args(ste, varprefix, funcdef, call, instance)

        # Introduce __returnval__; initialize it to None, equivalent to
        # implicit "return None" if there are no "Return" nodes:
        returnval = varprefix + "__returnval__"
        add_local(ste, returnval)
        # FIXME: this requires "None", how to do this in AST?
        assign = make_assignment(returnval,
                                 make_load_name('None', call), call)
        # FIXME: this leads to LOAD_NAME None, when it should be LOAD_CONST, surely?
        specialized.append(assign)

        # Make inline body, generating various statements
        # ending with:
        #    __inline____returnval = expr
        inline_body = []
        fixer = InlineBodyFixups(varprefix, funcdef.ste)
        for stmt in funcdef.body:
            assert isinstance(stmt, ast.AST)
            inline_body.append(fixer.visit(ast_clone(stmt)))
        #log('inline_body:', inline_body)
        specialized += inline_body

        #log('Parent: %s' % ast.dump(find_parent(self.tree, call)))

        # FIXME: need some smarts about the value of the "Specialize":
        # it's the final Expr within the body
        specialized_result = ast.copy_location(ast.Name(id=returnval,
                                                        ctx=ast.Load()),
                                               call)

        self.log('  specialized:')
        for stmt in specialized:
            self.log('    %s' % ast.dump(stmt))

        return ast.copy_location(ast.Specialize(name=call.func,
                                                expected_value='__internal__.saved.' + dotted_name,
                                                generalized=call,
                                                specialized_body=specialized,
                                                specialized_result=specialized_result),
                                 call)

        # Replace the call with a load from __inline____returnval__
        return ast.copy_location(ast.Name(id=returnval,
                                          ctx=ast.Load()), call)

class NotInlinable(Exception):
    pass

class CheckInlinableVisitor(PathTransformer):
    # Walk an ast.FunctionDef subtree, determining if it's inlinable
    def __init__(self, funcdef):
        self.funcdef = funcdef
        self.returns = []

    # Various nodes aren't handlable:
    def visit_FunctionDef(self, node, path):
        if node != self.funcdef:
            raise NotInlinable()
        self.generic_visit(node, path)
        return node
    def visit_ClassDef(self, node, path):
        raise NotInlinable()
    def visit_Yield(self, node, path):
        raise NotInlinable()
    def visit_Import(self, node, path):
        raise NotInlinable()
    def visit_ImportFrom(self, node, path):
        raise NotInlinable()

    def visit_Return(self, node, path):
        self.returns.append(path)
        return node

def fn_is_inlinable(funcdef, mod):
    # Should we inline calls to the given FunctionDef ?
    assert(isinstance(funcdef, ast.FunctionDef))

    # Only inline "simple" calling conventions for now:
    if len(funcdef.decorator_list) > 0:
        return False

    if (funcdef.args.vararg is not None or
        funcdef.args.kwarg is not None or
        funcdef.args.kwonlyargs != [] or
        funcdef.args.defaults != [] or
        funcdef.args.kw_defaults != []):
        return False

    # Don't try to inline generators and other awkward cases:
    v = CheckInlinableVisitor(funcdef)
    try:
        v.visit(funcdef)
    except NotInlinable:
        return False

    # TODO: restrict to just those functions with only a "return" at
    # the end (or implicit "return None"), no "return" in awkward places
    # (but could have other control flow)
    # Specifically: no returns in places that have successor code
    # for each return:
    # FIXME: for now, only inline functions which have a single, final
    # explicit "return" at the end, or no returns:
    log('returns of %s: %r' % (funcdef.name, v.returns))
    if len(v.returns)>1:
        return False

    # Single "return"?  Then it must be at the top level
    if len(v.returns) == 1:
        rpath = v.returns[0]
        # Must be at toplevel:
        if len(rpath.entries) != 1:
            return False

        # Must be at the end of that level
        if rpath.entries[0].index != len(funcdef.body)-1:
            return False



    # Don't inline functions that use free or cell vars
    # (just locals and globals):
    assert hasattr(funcdef, 'ste')
    ste = funcdef.ste
    for varname in ste.varnames:
        scope = get_scope(ste, varname)
        #log('%r: %r' % (varname, scope))
        if scope not in {LOCAL, GLOBAL_EXPLICIT, GLOBAL_IMPLICIT}:
            return False

        # Don't inline functions that use the "locals" or "vars" builtins:
        if varname in BUILTINS_THAT_READ_LOCALS:
            return False

    # Don't inline functions with names that get rebound:
    for node in ast.walk(mod):
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    if target.id == funcdef.name:
                        if isinstance(target.ctx, ast.Store):
                            return False

    return True



class InlinableFunctionFinder(PathTransformer):
    # Locate function definitions that look inlinable, recording, and adding globals:
    def __init__(self, mod):
        self.mod = mod
        self.funcdefs = {}

    def log(self, msg):
        if 0:
            print('%s: %s' % (self.__class__.__name__, msg))

    def visit_FunctionDef(self, funcdef, path):
        self.log('got function def: %r %r' % (funcdef.name, path))
        self.generic_visit(funcdef, path)
        if fn_is_inlinable(funcdef, self.mod):
            dotted_name = path.get_dotted_name(funcdef)
            self.log('using dotted name: %r for %s' % (dotted_name, path))
            self.funcdefs[dotted_name] = funcdef

            storedname = '__internal__.saved.' + dotted_name
            global_ = ast.copy_location(ast.Global(names=[storedname]),
                                            funcdef)
            assign = ast.copy_location(ast.Assign(targets=[ast.Name(id=storedname, ctx=ast.Store())],
                                                  value=ast.Name(id=funcdef.name, ctx=ast.Load())),
                                       funcdef)
            ast.fix_missing_locations(assign)

            return [funcdef, global_, assign]
        else:
            return funcdef


def _inline_function_calls(t):
    v = InlinableFunctionFinder(t)
    v.visit(t)
    def_dict = v.funcdefs

    # print('def_dict:%r' % def_dict)

    # Locate call sites:
    inliner = FunctionInliner(t, def_dict)
    inliner.visit(t)

    return t


def is_local_name(ste, expr):
    assert isinstance(expr, ast.AST)
    if isinstance(expr, ast.Name):
        if get_scope(ste, expr.id) == LOCAL:
            return True

def is_write_to_local(ste, expr):
    assert isinstance(expr, ast.AST)
    if is_local_name(ste, expr):
        if isinstance(expr.ctx, ast.Store):
            return True

def is_read_from_local(ste, expr):
    assert isinstance(expr, ast.AST)
    if is_local_name(ste, expr):
        if isinstance(expr.ctx, ast.Load):
            return True

def is_constant(expr):
    assert isinstance(expr, ast.AST)
    if isinstance(expr, (ast.Num, ast.Str, ast.Bytes)):
        return True


# The following optimizations currently can only cope with functions with a
# single basic-block, to avoid the need to build a CFG and do real data flow
# analysis.

class MoreThanOneBasicBlock(Exception):
    pass

class LocalAssignmentWalker(ast.NodeTransformer):
    def __init__(self, funcdef):
        self.funcdef = funcdef
        self.local_values = {}

    def log(self, msg):
        if 0:
            print(msg)

    def _is_local(self, varname):
        ste = self.funcdef.ste
        return get_scope(ste, varname) == LOCAL

    def _is_propagatable(self, expr):
        # Is this expression propagatable to successive store operations?

        # We can propagate reads of locals (until they are written to):
        if is_read_from_local(self.funcdef.ste, expr):
            return True

        if is_constant(expr):
            return True

        return False

    def visit_Assign(self, assign):
        self.generic_visit(assign)
        self.log('  got assign: %s' % ast.dump(assign))

        # Keep track of assignments to locals that are directly of constants
        # or of other other locals:
        for target in assign.targets:
            if is_write_to_local(self.funcdef.ste, target):
                self.log('    write to %r <- %s' % (target.id, ast.dump(assign.value)))
                if len(assign.targets) == 1:
                    target = assign.targets[0]
                    if self._is_propagatable(assign.value):
                        self.log('      recording value for %r: %s' % (target.id, ast.dump(assign.value)))
                        self.local_values[target.id] = assign.value
                        continue
                self.log('      %r is no longer known' % target.id)
                self.local_values[target.id] = None

        # Propagate earlier copies to this assignment:
        if len(assign.targets) == 1:
            target = assign.targets[0]
            if is_write_to_local(self.funcdef.ste, target):
                if target.id in self.local_values:
                    value = self.local_values[target.id]
                    if value is not None:
                        pass
                    #self.log('      copy-propagation target')

        return assign

    def visit_Name(self, name):
        self.log('visit_Name %r' % name)
        self.generic_visit(name)
        if is_read_from_local(self.funcdef.ste, name):
            self.log('  got read from local: %s' % ast.dump(name))
            if name.id in self.local_values:
                value = self.local_values[name.id]
                if value is not None:
                    self.log('      copy-propagating: %r <- %s' % (name.id, ast.dump(value)))
                    return value # clone this?
        return name

    # Nodes implying branching and looping:
    def visit_For(self, node):
        raise MoreThanOneBasicBlock()
    def visit_While(self, node):
        raise MoreThanOneBasicBlock()
    def visit_If(self, node):
        raise MoreThanOneBasicBlock()
    def visit_With(self, node):
        raise MoreThanOneBasicBlock()
    def visit_TryExcept(self, node):
        raise MoreThanOneBasicBlock()
    def visit_TryFinally(self, node):
        raise MoreThanOneBasicBlock()
    def visit_IfExp(self, node):
        raise MoreThanOneBasicBlock()


class CopyPropagation(ast.NodeTransformer):

    def visit_FunctionDef(self, funcdef):
        log('CopyPropagation: got function def: %r' % funcdef.name)
        self.generic_visit(funcdef)

        try:
            w = LocalAssignmentWalker(funcdef)
            w.visit(funcdef)
        except MoreThanOneBasicBlock:
            pass

        return funcdef


def _copy_propagation(t):
    # Very simple copy propagation, which (for simplicity), requires that we
    # have a single basic block
    v = CopyPropagation()
    v.visit(t)
    return t




class ReferenceToLocalFinder(PathTransformer):
    # Gather all reads/writes of locals within the given FunctionDef
    def __init__(self, funcdef):
        assert isinstance(funcdef, ast.FunctionDef)
        self.funcdef = funcdef
        # varnames of locals:
        self.local_reads = set()
        self.local_writes = set()
        self.globals = set()

    def log(self, msg):
        if 0:
            print(msg)

    def visit_Name(self, node, path):
        scope = get_scope(self.funcdef.ste, node.id)
        if scope == LOCAL:
            self.log('  found local: %r %r' % (ast.dump(node), path))
            if isinstance(node.ctx, ast.Store):
                self.local_writes.add(node.id)
            elif isinstance(node.ctx, ast.Load):
                self.local_reads.add(node.id)
            else:
                assert isinstance(node.ctx, ast.Del) # FIXME: what about other cases?
        elif scope in {GLOBAL_EXPLICIT, GLOBAL_IMPLICIT}:
            self.globals.add(node.id)
        return node

    def visit_AugAssign(self, augassign, path):
        # VAR += EXPR references VAR, and we can't remove it since it could
        # have arbitrary sideeffects
        if isinstance(augassign.target, ast.Name):
            target = augassign.target
            scope = get_scope(self.funcdef.ste, target.id)
            if scope == LOCAL:
                self.log('  found local: %r %r' % (ast.dump(augassign), path))
                # An augassign is both a read and a write:
                self.local_writes.add(target.id)
                self.local_reads.add(target.id)
        self.generic_visit(augassign, path)
        return augassign


class RemoveAssignmentToUnusedLocals(ast.NodeTransformer):
    # Replace all Assign([local], expr) with Expr(expr) for the given locals
    def __init__(self, varnames):
        self.varnames = varnames

    def visit_Assign(self, node):
        if len(node.targets) == 1:
            if isinstance(node.targets[0], ast.Name):
                if node.targets[0].id in self.varnames:
                    if isinstance(node.targets[0].ctx, ast.Store):
                        # Eliminate the assignment
                        return ast.copy_location(ast.Expr(node.value),
                                                 node.value)
                        # FIXME: also eliminate the variable from symtable
        return node



class RedundantLocalRemover(PathTransformer):

    def log(self, msg):
        if 0:
            print('%s: %s' % (self.__class__.__name__, msg))

    def visit_FunctionDef(self, funcdef, path):
        self.log('got function def: %r' % funcdef.name)
        v = ReferenceToLocalFinder(funcdef)
        v.visit(funcdef, path)
        self.generic_visit(funcdef, path)

        # Don't ellide locals if this function references the builtins
        # that access them:
        if v.globals.intersection(BUILTINS_THAT_READ_LOCALS):
            return funcdef

        unused_writes = v.local_writes - v.local_reads
        if unused_writes:
            self.log('    globals: %s' % v.globals)
            self.log('    loaded from: %s' % v.local_reads)
            self.log('    stored to: %s:' % v.local_writes)
            self.log('    unused: %s' % unused_writes)
            v = RemoveAssignmentToUnusedLocals(unused_writes)
            v.visit(funcdef)
        return funcdef

def _remove_redundant_locals(t):
    v = RedundantLocalRemover()
    v.visit(t)
    return t




# For now restrict ourselves to just a few places:
def is_test_code(t, filename):
    if filename == 'optimizable.py':
        return True
    for n in ast.walk(t):
        if isinstance(n, ast.FunctionDef):
            if n.name == 'function_to_be_inlined':
                return True
    return False

def dump_dot(t, filename):
    return False
    for n in ast.walk(t):
        if isinstance(n, ast.FunctionDef):
            if n.name == 'simple_method':
                return True
    return False

#class OptimizationError(Exception):
#    def __init__(self

from pprint import pprint
def optimize_ast(t, filename, st_blocks):
    if 0:
        print("optimize_ast called: %s" % filename)
    if is_test_code(t, filename):
        dot_before = to_dot(t)
        try:
            # pprint(st_blocks)
            # log(t)
            # pprint(t)
            # log(ast.dump(t))

            if isinstance(t, ast.Module):
                if dump_dot(t, filename):
                    dot_to_png(to_dot(t), 'before.png')

                t = _inline_function_calls(t)
                #cfg = CFG.from_ast(t)
                #print(cfg.to_dot())
                #dot_to_png(cfg.to_dot(), 'cfg.png')

                t = _copy_propagation(t)

                t = _remove_redundant_locals(t)

                if dump_dot(t, filename):
                    dot_to_png(to_dot(t), 'after.png')

        except:
            print('Exception during optimization of %r' % filename)
            # dot_to_png(dot_before, 'before.png')
            raise
    if 0:
        print('finished optimizing')
        if filename == 'optimizable.py':
            print(ast.dump(t))
    return t
