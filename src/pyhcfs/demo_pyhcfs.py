# -*- coding: utf-8 -*-

import pprint
from parser import *

_escape = dict((q, dict((c, unicode(repr(chr(c)))[1:-1])
    for c in range(32) + [ord('\\')] +
    range(128, 161),
    **{ord(q): u'\\' + q}))
    for q in ["'", '"'])
class MyPrettyPrinter(pprint.PrettyPrinter):
    def format(self, object, context, maxlevels, level):
	if type(object) is unicode:
	    q = "'" if "'" not in object or '"' in object \
		    else '"'
	    return ("u" + q + object.translate(_escape[q]) +
		    q, True, False)
	return pprint.PrettyPrinter.format(
		self, object, context, maxlevels, level)
pp = MyPrettyPrinter(indent=4)

print "\nlist_external_volume"
pprint.pprint(list_external_volume("testdata/fsmgr"))

print "\nhcfs_parse_meta"
pprint.pprint(hcfs_parse_meta("testdata/meta123"))

print "\nhcfs_list_dir_inorder"
offset, resp = hcfs_list_dir_inorder("testdata/meta123", limit=2)
pp.pprint((offset, resp))
