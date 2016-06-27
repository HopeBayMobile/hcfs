# -*- coding: utf-8 -*-

import pprint
from parser import *

pp = pprint.PrettyPrinter(indent=2)

# print "=============================="
# print "list_external_volume"
pp.pprint(list_external_volume(b"../../tests/unit_test/python/test_nexus_5x/fsmgr"))

# print "=============================="
# print "parse_meta"
pp.pprint(parse_meta(b"../../tests/unit_test/python/test_nexus_5x/meta"))
# pprint.pprint(parse_meta("testdata/metaXxx"))

# print "=============================="
# print "list_dir_inorder"
ret = { 'offset': (0, 0) }
sum = 0
while True:
    ret = list_dir_inorder(b"../../tests/unit_test/python/test_nexus_5x/meta",
            ret['offset'], limit=33)
    files = [ x['d_name'] for x in ret['child_list'] ]
    if len(files) == 0:
        break
    print(files)
    sum += len(files)
    print("Total", sum, "files")
