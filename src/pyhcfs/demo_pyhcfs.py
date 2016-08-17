# -*- coding: utf-8 -*-

import pprint
import textwrap
from parser import *

pp = pprint.PrettyPrinter(indent=2)
def demo_title(title):
    print("")
    print("Demo", title)
    print("==============================")

def demo(cmd):
    prefix="    "
    print("\n" + prefix + cmd + "\n")
    exec('print(textwrap.indent(pp.pformat('+cmd+'), prefix))')

test_target='test_data/v1/android'

demo_title("list_external_volume")
demo('list_external_volume(b"'+test_target+'/fsmgr")')

demo_title("list_external_volume (Failure)")
demo('list_external_volume(b"")')

demo_title("parse_meta")
demo('parse_meta(b"'+test_target+'/meta_isdir")')

demo_title("parse_meta (Failure)")
demo('parse_meta(b"test_data/v0/android/meta_isreg")')

ret = { 'offset': (0, 0) }
demo_title("list_dir_inorder")
demo('list_dir_inorder(b"'+test_target+'/meta_isdir", ret["offset"], limit=100)')

demo_title("get_vol_usage")
demo('get_vol_usage(b"'+test_target+'/FSstat")')

demo_title("list_file_blocks")
demo('list_file_blocks(b"'+test_target+'/meta_isreg")')
