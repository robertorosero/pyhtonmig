import unittest
from db.sqlite.test import (dbapi, types, userfunctions, 
                                factory, transactions)

def suite():
    return unittest.TestSuite(
        (dbapi.suite(), types.suite(), userfunctions.suite(), 
         factory.suite(), transactions.suite()))

def test():
    runner = unittest.TextTestRunner()
    runner.run(suite())

if __name__ == "__main__":
    test()
