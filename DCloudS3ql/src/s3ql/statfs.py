'''
statfs.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) 2008-2009 Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
File modified by Jiahong Wu to include info on local cache and status
'''

from __future__ import division, print_function, absolute_import
from .common import CTRL_NAME, QuietError, setup_logging
from .parse_args import ArgumentParser
import llfuse
import logging
import os
import posixpath
import struct
import sys


log = logging.getLogger("stat")

def parse_args(args):
    '''Parse command line'''

    parser = ArgumentParser(
        description="Print file system statistics.")

    parser.add_debug()
    parser.add_quiet()
    parser.add_version()
    parser.add_argument("mountpoint", metavar='<mountpoint>',
                        type=(lambda x: x.rstrip('/')),
                        help='Mount point of the file system to examine')

    return parser.parse_args(args)

def main(args=None):
    '''Print file system statistics to sys.stdout'''

    if args is None:
        args = sys.argv[1:]

    options = parse_args(args)
    setup_logging(options)
    mountpoint = options.mountpoint

    # Check if it's a mount point
    if not posixpath.ismount(mountpoint):
        raise QuietError('%s is not a mount point' % mountpoint)

    # Check if it's an S3QL mountpoint
    ctrlfile = os.path.join(mountpoint, CTRL_NAME)
    if not (CTRL_NAME not in llfuse.listdir(mountpoint)
            and os.path.exists(ctrlfile)):
        raise QuietError('%s is not a mount point' % mountpoint)

    if os.stat(ctrlfile).st_uid != os.geteuid() and os.geteuid() != 0:
        raise QuietError('Only root and the mounting user may run s3qlstat.')

    # Use a decent sized buffer, otherwise the statistics have to be
    # calculated thee(!) times because we need to invoce getxattr
    # three times.
    buf = llfuse.getxattr(ctrlfile, b's3qlstat', size_guess=256)

    (entries, blocks, inodes, fs_size, dedup_size,
     compr_size, db_size)= struct.unpack('QQQQQQQ', buf) 

    # Added by Jiahong Wu: read cache information
    buf = llfuse.getxattr(ctrlfile, b's3qlcache', size_guess=256)
    (cache_size, cache_dirtysize, cache_entries, cache_dirtyentries, 
        cache_maxsize, cache_maxentries, cache_uploading, filesys_write, 
        quota_size, cache_openedentries) = struct.unpack('QQQQQQQQQQ', buf)
    p_dedup = dedup_size * 100 / fs_size if fs_size else 0
    p_compr_1 = compr_size * 100 / fs_size if fs_size else 0
    p_compr_2 = compr_size * 100 / dedup_size if dedup_size else 0
    mb = 1024 ** 2
    print ('Directory entries:    %d' % entries,
           'Inodes:               %d' % inodes,
           'Data blocks:          %d' % blocks,
           'Total data size:      %.2f MB' % (fs_size / mb),
           'After de-duplication: %.2f MB (%.2f%% of total)'
             % (dedup_size / mb, p_dedup),
           'After compression:    %.2f MB (%.2f%% of total, %.2f%% of de-duplicated)'
             % (compr_size / mb, p_compr_1, p_compr_2),
           'Database size:        %.2f MB (uncompressed)' % (db_size / mb),
           '(some values do not take into account not-yet-uploaded dirty blocks in cache)',
           'Cache size: current: %.2f MB, max: %.2f MB' % ((cache_size / mb),(cache_maxsize / mb)),
           'Cache entries: current: %d, max: %d' % (cache_entries,cache_maxentries),
           'Dirty cache status: size: %.2f MB, entries: %d' % ((cache_dirtysize / mb),cache_dirtyentries),
           'Opened cache entries: %d' % cache_openedentries,
           sep='\n')
    if cache_uploading == 1:
        print('Cache uploading: On\n')
    else:
        print('Cache uploading: Off\n')    

    if (cache_dirtysize > (0.8 * cache_maxsize)) or (cache_dirtyentries > (0.8 * cache_maxentries)):
        print('Dirty cache near full: True')
    else:
        print('Dirty cache near full: False')
    
    if filesys_write == 1:
        print('File system writing: On')
    else:
        print('File system writing: Off')
    
    print('Quota: %.2f MB\n' % (quota_size / mb))


if __name__ == '__main__':
    main(sys.argv[1:])
