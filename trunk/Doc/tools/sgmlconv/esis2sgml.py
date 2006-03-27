#! /usr/bin/env python

"""Convert ESIS events to SGML or XML markup.

This is limited, but seems sufficient for the ESIS generated by the
latex2esis.py script when run over the Python documentation.
"""

# This should have an explicit option to indicate whether the *INPUT* was
# generated from an SGML or an XML application.

import errno
import os
import re
import string

from xml.sax.saxutils import escape

import esistools


AUTOCLOSE = ()

EMPTIES_FILENAME = "../sgml/empties.dat"
LIST_EMPTIES = 0


_elem_map = {}
_attr_map = {}
_token_map = {}

_normalize_case = str

def map_gi(sgmlgi, map):
    uncased = _normalize_case(sgmlgi)
    try:
        return map[uncased]
    except IndexError:
        map[uncased] = sgmlgi
        return sgmlgi

def null_map_gi(sgmlgi, map):
    return sgmlgi


def format_attrs(attrs, xml=0):
    attrs = attrs.items()
    attrs.sort()
    parts = []
    append = parts.append
    for name, value in attrs:
        if xml:
            append('%s="%s"' % (name, escape(value)))
        else:
            # this is a little bogus, but should do for now
            if name == value and isnmtoken(value):
                append(value)
            elif istoken(value):
                if value == "no" + name:
                    append(value)
                else:
                    append("%s=%s" % (name, value))
            else:
                append('%s="%s"' % (name, escape(value)))
    if parts:
        parts.insert(0, '')
    return " ".join(parts)


_nmtoken_rx = re.compile("[a-z][-._a-z0-9]*$", re.IGNORECASE)
def isnmtoken(s):
    return _nmtoken_rx.match(s) is not None

_token_rx = re.compile("[a-z0-9][-._a-z0-9]*$", re.IGNORECASE)
def istoken(s):
    return _token_rx.match(s) is not None


def convert(ifp, ofp, xml=0, autoclose=(), verbatims=()):
    if xml:
        autoclose = ()
    attrs = {}
    lastopened = None
    knownempties = []
    knownempty = 0
    lastempty = 0
    inverbatim = 0
    while 1:
        line = ifp.readline()
        if not line:
            break

        type = line[0]
        data = line[1:]
        if data and data[-1] == "\n":
            data = data[:-1]
        if type == "-":
            data = esistools.decode(data)
            data = escape(data)
            if not inverbatim:
                data = data.replace("---", "&mdash;")
            ofp.write(data)
            if "\n" in data:
                lastopened = None
            knownempty = 0
            lastempty = 0
        elif type == "(":
            if data == "COMMENT":
                ofp.write("<!--")
                continue
            data = map_gi(data, _elem_map)
            if knownempty and xml:
                ofp.write("<%s%s/>" % (data, format_attrs(attrs, xml)))
            else:
                ofp.write("<%s%s>" % (data, format_attrs(attrs, xml)))
            if knownempty and data not in knownempties:
                # accumulate knowledge!
                knownempties.append(data)
            attrs = {}
            lastopened = data
            lastempty = knownempty
            knownempty = 0
            inverbatim = data in verbatims
        elif type == ")":
            if data == "COMMENT":
                ofp.write("-->")
                continue
            data = map_gi(data, _elem_map)
            if xml:
                if not lastempty:
                    ofp.write("</%s>" % data)
            elif data not in knownempties:
                if data in autoclose:
                    pass
                elif lastopened == data:
                    ofp.write("</>")
                else:
                    ofp.write("</%s>" % data)
            lastopened = None
            lastempty = 0
            inverbatim = 0
        elif type == "A":
            name, type, value = data.split(" ", 2)
            name = map_gi(name, _attr_map)
            attrs[name] = esistools.decode(value)
        elif type == "e":
            knownempty = 1
        elif type == "&":
            ofp.write("&%s;" % data)
            knownempty = 0
        else:
            raise RuntimeError, "unrecognized ESIS event type: '%s'" % type

    if LIST_EMPTIES:
        dump_empty_element_names(knownempties)


def dump_empty_element_names(knownempties):
    d = {}
    for gi in knownempties:
        d[gi] = gi
    knownempties.append("")
    if os.path.isfile(EMPTIES_FILENAME):
        fp = open(EMPTIES_FILENAME)
        while 1:
            line = fp.readline()
            if not line:
                break
            gi = line.strip()
            if gi:
                d[gi] = gi
    fp = open(EMPTIES_FILENAME, "w")
    gilist = d.keys()
    gilist.sort()
    fp.write("\n".join(gilist))
    fp.write("\n")
    fp.close()


def update_gi_map(map, names, fromsgml=1):
    for name in names.split(","):
        if fromsgml:
            uncased = name.lower()
        else:
            uncased = name
        map[uncased] = name


def main():
    import getopt
    import sys
    #
    autoclose = AUTOCLOSE
    xml = 1
    xmldecl = 0
    elem_names = ''
    attr_names = ''
    value_names = ''
    verbatims = ('verbatim', 'interactive-session')
    opts, args = getopt.getopt(sys.argv[1:], "adesx",
                               ["autoclose=", "declare", "sgml", "xml",
                                "elements-map=", "attributes-map",
                                "values-map="])
    for opt, arg in opts:
        if opt in ("-d", "--declare"):
            xmldecl = 1
        elif opt == "-e":
            global LIST_EMPTIES
            LIST_EMPTIES = 1
        elif opt in ("-s", "--sgml"):
            xml = 0
        elif opt in ("-x", "--xml"):
            xml = 1
        elif opt in ("-a", "--autoclose"):
            autoclose = arg.split(",")
        elif opt == "--elements-map":
            elem_names = ("%s,%s" % (elem_names, arg))[1:]
        elif opt == "--attributes-map":
            attr_names = ("%s,%s" % (attr_names, arg))[1:]
        elif opt == "--values-map":
            value_names = ("%s,%s" % (value_names, arg))[1:]
    #
    # open input streams:
    #
    if len(args) == 0:
        ifp = sys.stdin
        ofp = sys.stdout
    elif len(args) == 1:
        ifp = open(args[0])
        ofp = sys.stdout
    elif len(args) == 2:
        ifp = open(args[0])
        ofp = open(args[1], "w")
    else:
        usage()
        sys.exit(2)
    #
    # setup the name maps:
    #
    if elem_names or attr_names or value_names:
        # assume the origin was SGML; ignore case of the names from the ESIS
        # stream but set up conversion tables to get the case right on output
        global _normalize_case
        _normalize_case = string.lower
        update_gi_map(_elem_map, elem_names.split(","))
        update_gi_map(_attr_map, attr_names.split(","))
        update_gi_map(_values_map, value_names.split(","))
    else:
        global map_gi
        map_gi = null_map_gi
    #
    # run the conversion:
    #
    try:
        if xml and xmldecl:
            opf.write('<?xml version="1.0" encoding="iso8859-1"?>\n')
        convert(ifp, ofp, xml=xml, autoclose=autoclose, verbatims=verbatims)
    except IOError, (err, msg):
        if err != errno.EPIPE:
            raise


if __name__ == "__main__":
    main()
