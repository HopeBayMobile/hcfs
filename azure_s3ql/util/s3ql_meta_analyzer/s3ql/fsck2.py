'''
fsck.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
Jia-Hong marking this file on 4/16/2012 for future edits
Jia-Hong edited this file for checking cached files without uploading them first
'''

from __future__ import division, print_function, absolute_import
from .backends.common import NoSuchObject, get_bucket, NoSuchBucket, TimeoutError
from .common import (ROOT_INODE, inode_for_path, get_path, get_bucket_cachedir, 
    QuietError, stream_read_bz2, setup_logging)
from .database import NoSuchRowError, Connection
from .metadata import restore_metadata, create_tables
from .parse_args import ArgumentParser
from os.path import basename
import apsw
import cPickle as pickle
import logging
import os
import re
import shutil
import stat
import sys
import tempfile
import textwrap
import time
import subprocess
import resource

RESULT_LOG = '/result.log'

log = logging.getLogger("fsck2")

S_IFMT = (stat.S_IFDIR | stat.S_IFREG | stat.S_IFSOCK | stat.S_IFBLK |
          stat.S_IFCHR | stat.S_IFIFO | stat.S_IFLNK)

unlink_later = set()

class Fsck(object):
    def __init__(self, bucket_, conn):
        self.bucket = bucket_
        self.expect_errors = False
        self.found_errors = False
        self.uncorrectable_errors = False
        self.conn = conn
        self.cached_blocks = set()
        self.inode_size_cache = dict()

        # Set of blocks that have been unlinked by check_cache.
        # check_block_refcounts() will not report errors if these blocks still
        # exist even though they have refcount=0
        self.unlinked_blocks = set()

        # Similarly for objects
        self.unlinked_objects = set()

        # Set of inodes that have been moved to lost+found (so that we
        # don't move them there repeatedly)
        self.moved_inodes = set()
        self.fh = open(os.getcwd() + RESULT_LOG, 'w')
        
    def __del__(self):
        self.fh.close()

    def check(self):
        """Check file system
        
        Sets instance variable `found_errors`.
        """

        # Create indices required for reference checking
        log.info('Creating temporary extra indices...')
        for idx in ('tmp1', 'tmp2', 'tmp3', 'tmp4', 'tmp5'):
            self.conn.execute('DROP INDEX IF EXISTS %s' % idx)
        self.conn.execute('CREATE INDEX tmp1 ON blocks(obj_id)')
        self.conn.execute('CREATE INDEX tmp2 ON inode_blocks(block_id)')
        self.conn.execute('CREATE INDEX tmp3 ON contents(inode)')
        self.conn.execute('CREATE INDEX tmp4 ON contents(name_id)')
        self.conn.execute('CREATE INDEX tmp5 ON ext_attributes(name_id)')
        try:
            self.check_objects_id()
        finally:
            log.info('Dropping temporary indices...')
            for idx in ('tmp1', 'tmp2', 'tmp3', 'tmp4', 'tmp5'):
                self.conn.execute('DROP INDEX %s' % idx)
                
    def analyze_objs(self, backend_only_objs, frontend_only_objs):
        pass

    def log_error(self, *a, **kw):
        '''Log file system error if not expected'''

        if not self.expect_errors:
            return log.warn(*a, **kw)

    def check_objects_id(self):
        """Check objects.id"""
        global unlink_later

        log.info('Checking objects (backend)...')

        lof_id = self.conn.get_val("SELECT inode FROM contents_v "
                                   "WHERE name=? AND parent_inode=?", (b"lost+found", ROOT_INODE))
        backend_only_objs = set()
        frontend_only_objs = set()

        # We use this table to keep track of the objects that we have seen
        self.conn.execute("CREATE TEMP TABLE obj_ids (id INTEGER PRIMARY KEY)")
        try:
            for (i, obj_name) in enumerate(self.bucket.list('s3ql_data_')):

                if i != 0 and i % 5000 == 0:
                    log.info('..processed %d objects so far..', i)

                # We only bother with data objects
                try:
                    obj_id = int(obj_name[10:])
                    if obj_id in unlink_later:
                        continue
                except ValueError:
                    log.warn("Ignoring unexpected object %r", obj_name)
                    continue

                self.conn.execute('INSERT INTO obj_ids VALUES(?)', (obj_id,))

            for (obj_id,) in self.conn.query('SELECT id FROM obj_ids '
                                             'EXCEPT SELECT id FROM objects'):
                try:
                    backend_only_objs.add(obj_id)
                    self.log_error("Deleted spurious object %d", obj_id)
                except NoSuchObject:
                    pass
                # Yuxun, handle timeout error
                except TimeoutError:
                    pass

            self.conn.execute('CREATE TEMPORARY TABLE missing AS '
                              'SELECT id FROM objects EXCEPT SELECT id FROM obj_ids')

            for (obj_id,) in self.conn.query('SELECT * FROM missing'):
                if ('s3ql_data_%d' % obj_id) in self.bucket:
                    # Object was just not in list yet
                    continue

                self.found_errors = True
                self.log_error("object %s only exists in table but not in bucket, deleting", obj_id)
                frontend_only_objs.add(obj_id)

                #Jiahong: (4/27/12) Modifying the following database query to return both the inode and blockno
                for (id_, blockno) in self.conn.query('SELECT inode, blockno FROM inode_blocks JOIN blocks ON block_id = id '
                                              'WHERE obj_id=? ', (obj_id,)):
                    for (name, name_id, id_p) in self.conn.query('SELECT name, name_id, parent_inode '
                                                                 'FROM contents_v WHERE inode=?', (id_,)):
                        path = get_path(id_p, self.conn, name)
                        self.log_error("File may lack data, moved to /lost+found: %s", path)
        finally:
            self.conn.execute('DROP TABLE obj_ids')
            self.conn.execute('DROP TABLE IF EXISTS missing')
            
            # analyze these objects
            self.analyze_objs(backend_only_objs, frontend_only_objs)
            
            # dump obj list to file
            # with open("/root/s3ql/objects_to_delete", "w") as fh:
                # fh.write("obj_only_in_backend,")
                # for obj_id in backend_only_objs:
                    # fh.write("%s," % obj_id)
                # fh.write("\n")
                # fh.write("obj_only_in_frontend,")
                # for obj_id in frontend_only_objs:
                    # fh.write("%s," % obj_id)
                # fh.write("\n")

    # def resolve_free(self, path, name):
        # '''Return parent inode and name of an unused directory entry
        
        # The directory entry will be in `path`. If an entry `name` already
        # exists there, we append a numeric suffix.
        # '''

        # if not isinstance(path, bytes):
            # raise TypeError('path must be of type bytes')

        # inode_p = inode_for_path(path, self.conn)

        # # Debugging http://code.google.com/p/s3ql/issues/detail?id=217
        # # and http://code.google.com/p/s3ql/issues/detail?id=261
        # if len(name) > 255 - 4:
            # name = '%s ... %s' % (name[0:120], name[-120:])

        # i = 0
        # newname = name
        # name += b'-'
        # try:
            # while True:
                # self.conn.get_val("SELECT inode FROM contents_v "
                                  # "WHERE name=? AND parent_inode=?", (newname, inode_p))
                # i += 1
                # newname = name + bytes(i)

        # except NoSuchRowError:
            # pass

        # return (inode_p, newname)

    # def _add_name(self, name):
        # '''Get id for *name* and increase refcount
        
        # Name is inserted in table if it does not yet exist.
        # '''

        # try:
            # name_id = self.conn.get_val('SELECT id FROM names WHERE name=?', (name,))
        # except NoSuchRowError:
            # name_id = self.conn.rowid('INSERT INTO names (name, refcount) VALUES(?,?)',
                                      # (name, 1))
        # else:
            # self.conn.execute('UPDATE names SET refcount=refcount+1 WHERE id=?', (name_id,))
        # return name_id

    # def _del_name(self, name_id):
        # '''Decrease refcount for name_id, remove if it reaches 0'''

        # self.conn.execute('UPDATE names SET refcount=refcount-1 WHERE id=?', (name_id,))
        # self.conn.execute('DELETE FROM names WHERE refcount=0 AND id=?', (name_id,))

def parse_args(args):

    parser = ArgumentParser(
        description="Checks and repairs an S3QL filesystem.")

    parser.add_log(os.getcwd() + RESULT_LOG)
    #parser.add_cachedir()
    parser.add_authfile()
    parser.add_debug_modules()
    parser.add_quiet()
    parser.add_ssl()
    parser.add_version()
    parser.add_storage_url()

    parser.add_argument("--batch", action="store_true", default=False,
                      help="If user input is required, exit without prompting.")
    parser.add_argument("--force", action="store_true", default=False,
                      help="Force checking even if file system is marked clean.")
    options = parser.parse_args(args)

    return options

def download_metadata(bucket, cachepath):
    """ Downloading and decompressing metadata from swift"""
    def do_read(fh):
        tmpfh = tempfile.TemporaryFile()
        stream_read_bz2(fh, tmpfh)
        return tmpfh
    log.info('Downloading and decompressing metadata...')
    tmpfh = bucket.perform_read(do_read, "s3ql_metadata")
    os.close(os.open(cachepath + '.db.tmp', os.O_RDWR | os.O_CREAT | os.O_TRUNC,
                     stat.S_IRUSR | stat.S_IWUSR))
    db = Connection(cachepath + '.db.tmp', fast_mode=True)
    log.info("Reading metadata...")
    tmpfh.seek(0)
    restore_metadata(tmpfh, db)
    db.close()
    os.rename(cachepath + '.db.tmp', cachepath + '.db')

def main(args=None):
    if args is None:
        args = sys.argv[1:]

    global unlink_later

    options = parse_args(args)
    stdout_log_handler = setup_logging(options)

    try:
        bucket = get_bucket(options)
    except NoSuchBucket as exc:
        raise QuietError(str(exc))
    except:
        log.info('[During Startup] Connection failed. Retrying after 10 seconds...')
        time.sleep(10)
        try:
            bucket = get_bucket(options)
        except NoSuchBucket as exc:
            raise QuietError(str(exc))
        except Exception as exc:
            # return code 128 to indicate "wrong enc key"
            if (str(exc).find('Wrong bucket passphrase') != -1):
                return 128
            log.error("Unable to connect to backend. Please check network connection then reboot to retry. If this cannot solve your problem, please contact service people.")
            raise QuietError(str(exc))

    cachepath = get_bucket_cachedir(options.storage_url, '/tmp')
    param_remote = bucket.lookup('s3ql_metadata')
    db = None

    download_metadata(bucket, cachepath)
    db = Connection(cachepath + '.db')

    fsck = Fsck(bucket, db)
    fsck.check()

    db.close()
    # we don't need s3ql metadata anymore
    os.system('sudo rm %s' % cachepath + '.db')

if __name__ == '__main__':
    main(sys.argv[1:])
