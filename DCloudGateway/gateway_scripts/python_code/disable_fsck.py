#!/usr/bin/env python
"""
    Utility to change /etc/fstab to disable fsck at boot up time.
"""
import os
import time
import re

FSTAB_FILE = '/etc/fstab'

# backup /etc/fstab
backup_file = '/root/fstab.%d' % int(time.time())
print('Backing up %s to %s' % (FSTAB_FILE, backup_file))
os.system('sudo cp %s /root/fstab.%d' % (FSTAB_FILE, int(time.time())))

try:
    lines = None
    # read file
    with open(FSTAB_FILE, 'r') as fh:
        lines = fh.readlines()

    revised_lines = ""
    # go through every line
    for line in lines:
        if line.startswith('UUID'):
            # modify the last number to zero
            tokens = line.split(' ')
            tokens[-1] = '0\n'
            line2 = ' '.join(tokens)
            revised_lines += line2
        else:
            revised_lines += line
            
    # write file
    with open(FSTAB_FILE, 'w') as fh:
        fh.write(revised_lines)
    print('Written revised %s.' % FSTAB_FILE)
except Exception as e:
    print(str(e))