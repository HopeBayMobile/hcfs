# -*- coding: utf-8 -*-

import pprint
from parser import *

pp = pprint.PrettyPrinter(indent=4)

# print "=============================="
# print "list_external_volume"
pprint.pprint(list_external_volume(b"testdata/fsmgr"))

# print "=============================="
# print "parse_meta"
pprint.pprint(parse_meta(b"testdata/meta423"))
# pprint.pprint(parse_meta("testdata/metaXxx"))

# print "=============================="
# print "list_dir_inorder"
offset = (0, 0)
ret = list_dir_inorder(b"testdata/meta423", offset, limit=50)
pp.pprint(ret)
ret = list_dir_inorder(b"testdata/meta423", ret['offset'], limit=50)
pp.pprint(ret)
ret = list_dir_inorder(b"testdata/meta423", ret['offset'], limit=50)
pp.pprint(ret)
