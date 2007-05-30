"""Importing a module that has the same name as one that is supposed to be
blocked (e.g., sys) should be okay."""
import sys
sys.test_attr
