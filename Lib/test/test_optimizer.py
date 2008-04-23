import _ast
import unittest
from test import test_support

class AstOptimizerTest(unittest.TestCase):
    def compileast(self, code):
        return compile(code, "<string>", "exec", _ast.PyCF_ONLY_AST)

    def test_remove_not_from_if_stmt(self):
        # ensure we can optimize a boolean "not" out of if statements
        code = """

x = 10

if not x:
    1
else:
    2

"""
        ast = self.compileast(code)
        self.assertEqual(2, len(ast.body))
        self.assertEqual(_ast.If, ast.body[1].__class__)
        self.assertEqual(_ast.Name, ast.body[1].test.__class__)
        self.assertEqual(2, ast.body[1].body[0].value.n)
        self.assertEqual(1, ast.body[1].orelse[0].value.n)

    def test_fold_if_stmt_with_constants(self):
        # ensure we can optimize conditionals using simple constants
        code = """
if 1:
    'true'
else:
    'false'

if 0:
    'true'
else:
    'false'

"""
        ast = self.compileast(code)
        self.assertEqual(2, len(ast.body))
        self.assertEqual(_ast.Str, ast.body[0].value.__class__)
        self.assertEqual('true', ast.body[0].value.s)
        self.assertEqual(_ast.Str, ast.body[1].value.__class__)
        self.assertEqual('false', ast.body[1].value.s)

    def test_fold_unary_op_before_collapse_branch(self):
        # ensure unary op folding is applied before collapsing a branch
        code = """
if not 1:
    'true'
else:
    'false'
"""
        ast = self.compileast(code)
        self.assertEqual(1, len(ast.body))
        self.assertEqual(_ast.Str, ast.body[0].value.__class__)
        self.assertEqual('false', ast.body[0].value.s)

    def assertAstNode(self, expected_type, attr, expected_value, code):
        ast = self.compileast(code)
        self.assertEqual(1, len(ast.body))
        self.assertEqual(expected_type, ast.body[0].value.__class__)
        if None not in (attr, expected_value):
            self.assertEqual(expected_value, getattr(ast.body[0].value, attr))

    def assertNum(self, expected, code):
        return self.assertAstNode(_ast.Num, 'n', expected, code)

    def assertStr(self, expected, code):
        return self.assertAstNode(_ast.Str, 's', expected, code)

    def test_binop_fold_num(self):
        tests = (
            ("1 + 2",         3),
            ("'@'*4",         "@@@@"),
            ("'abc' + 'def'", "abcdef"),
            ("3**4",          81),
            ("3*4",           12),
            ("13//4",         3),
            ("14%4",          2),
            ("2+3",           5),
            ("13-4",          9),
            ("13<<2",         52),
            ("13>>2",         3),
            ("13&7",          5),
            ("13^7",          10),
            ("13|7",          15)
        )

        attrmap = {
            int: 'n',
            str: 's'
        }

        for code, expected in tests:
            ast = self.compileast(code)
            actual = getattr(ast.body[0].value, attrmap[type(expected)])
            self.assertEqual(expected, actual)

    def test_binop_fold_num_with_variable(self):
        # check binary constant folding occurs even where
        # only partial folding is possible

        # XXX: this will work for x + 3 * 2, but for x + 3 + 2
        # the constant folding will not occur because the parser builds
        # something like (+ (+ x 1) 2): need to be more aggressive with these!

        code = """
x = 5
x + 3 * 2
"""
        ast = self.compileast(code)
        self.assertEqual(2, len(ast.body))
        self.assertEqual(_ast.Assign, ast.body[0].__class__)
        self.assertEqual(_ast.Expr, ast.body[1].__class__)
        self.assertEqual(_ast.BinOp, ast.body[1].value.__class__)
        self.assertEqual(_ast.Name, ast.body[1].value.left.__class__)
        self.assertEqual(_ast.Num, ast.body[1].value.right.__class__)
        self.assertEqual(6, ast.body[1].value.right.n)

    def test_unary_fold_num(self):
        # check unary constant folding for numeric values
        self.assertNum(-5, "-5")
        self.assertNum(True, "not 0")
        self.assertNum(-3, "-+3")
        self.assertNum(False, "not True")
        self.assertNum(True, "not None")

    def test_return_none_becomes_return(self):
        code = """
def foo():
    return None
"""
        ast = self.compileast(code)
        self.assertEqual(_ast.Return, ast.body[0].body[0].__class__)
        self.assertEqual(None, ast.body[0].body[0].value)

    def test_yield_none_becomes_yield(self):
        code = """
def foo():
    yield None
"""
        ast = self.compileast(code)
        self.assertEqual(_ast.Expr, ast.body[0].body[0].__class__)
        self.assertEqual(_ast.Yield, ast.body[0].body[0].value.__class__)
        self.assertEqual(None, ast.body[0].body[0].value.value)

    def test_eliminate_code_after_return(self):
        # ensure code following a "return" is erased from the AST
        code = """
def say_hello():
    print "Hello there"
    return True
    print "A secret message!"
"""
        ast = self.compileast(code)
        self.assertEqual(1, len(ast.body))
        self.assertEqual(_ast.FunctionDef, ast.body[0].__class__)
        self.assertEqual(3, len(ast.body[0].body))
        self.assertEqual(_ast.Pass, ast.body[0].body[2].__class__)

def test_main():
    test_support.run_unittest(AstOptimizerTest)

if __name__ == '__main__':
    test_main()
