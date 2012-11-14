'''
fs.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) 2008-2009 Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
Jia-Hong marking this file on 4/16/2012 for future edits
'''

from __future__ import division, print_function, absolute_import
from .backends.common import NoSuchObject, ChecksumError
from .common import (get_path, CTRL_NAME, CTRL_INODE, LoggerFilter)
from .database import NoSuchRowError
from .inode_cache import OutOfInodesError
from . import deltadump
from cStringIO import StringIO
from llfuse import FUSEError
import cPickle as pickle
import collections
import errno
import llfuse
import logging
import math
import os
import stat
import struct
import time

# standard logger for this module
log = logging.getLogger("fs")

# For long requests, we force a GIL release in the following interval
GIL_RELEASE_INTERVAL = 0.05
REFRESH_COUNT = 1080

class Operations(llfuse.Operations):
    """A full-featured file system for online data storage

    This class implements low-level FUSE operations and is meant to be passed to
    llfuse.init().
    
    The ``access`` method of this class always gives full access, independent of
    file permissions. If the FUSE library is initialized with ``allow_other`` or
    ``allow_root``, the ``default_permissions`` option should therefore always
    be passed as well.
    
    
    Attributes:
    -----------

    :cache:       Holds information about cached blocks
    :inode_cache: A cache for the attributes of the currently opened inodes.
    :open_inodes: dict of currently opened inodes. This is used to not remove
                  the blocks of unlinked inodes that are still open.
    :upload_event: If set, triggers a metadata upload
 
    Multithreading
    --------------
    
    All methods are reentrant and may release the global lock while they
    are running.
    
 
    Directory Entry Types
    ----------------------
    
    S3QL is quite agnostic when it comes to directory entry types. Every
    directory entry can contain other entries *and* have a associated data,
    size, link target and device number. However, S3QL makes some provisions for
    users relying on unlink()/rmdir() to fail for a directory/file. For that, it
    explicitly checks the st_mode attribute.
    """

    def __init__(self, block_cache, db, max_obj_size, inode_cache,
                 upload_event=None):
        super(Operations, self).__init__()

        self.inodes = inode_cache
        self.db = db
        self.upload_event = upload_event
        self.open_inodes = collections.defaultdict(lambda: 0)
        self.max_obj_size = max_obj_size
        self.cache = block_cache

    def destroy(self):
        self.forget(self.open_inodes.items())
        self.inodes.destroy()

    def lookup(self, id_p, name):
        log.debug('lookup(%d, %r): start', id_p, name)

        if name == CTRL_NAME:
            inode = self.inodes[CTRL_INODE]

            # Make sure the control file is only writable by the user
            # who mounted the file system (but don't mark inode as dirty)
            object.__setattr__(inode, 'uid', os.getuid())
            object.__setattr__(inode, 'gid', os.getgid())

        elif name == '.':
            inode = self.inodes[id_p]

        elif name == '..':
            id_ = self.db.get_val("SELECT parent_inode FROM contents WHERE inode=?",
                                  (id_p,))
            inode = self.inodes[id_]

        else:
            try:
                id_ = self.db.get_val("SELECT inode FROM contents_v WHERE name=? AND parent_inode=?",
                                      (name, id_p))
            except NoSuchRowError:
                raise(llfuse.FUSEError(errno.ENOENT))
            inode = self.inodes[id_]

        self.open_inodes[inode.id] += 1
        return inode

    def getattr(self, id_):
        log.debug('getattr(%d): start', id_)
        if id_ == CTRL_INODE:
            # Make sure the control file is only writable by the user
            # who mounted the file system (but don't mark inode as dirty)
            inode = self.inodes[CTRL_INODE]
            object.__setattr__(inode, 'uid', os.getuid())
            object.__setattr__(inode, 'gid', os.getgid())
            return inode

        return self.inodes[id_]

    def readlink(self, id_):
        log.debug('readlink(%d): start', id_)
        timestamp = time.time()
        inode = self.inodes[id_]
        if inode.atime < inode.ctime or inode.atime < inode.mtime:
            inode.atime = timestamp
        try:
            return self.db.get_val("SELECT target FROM symlink_targets WHERE inode=?", (id_,))
        except NoSuchRowError:
            log.warn('Inode does not have symlink target: %d', id_)
            raise FUSEError(errno.EINVAL)

    def opendir(self, id_):
        log.debug('opendir(%d): start', id_)
        return id_

    def check_args(self, args):
        '''Check and/or supplement fuse mount options'''

        #Jiahong: Commented out max_write config here as new config is added to mount.py
        args.append(b'big_writes')
        #args.append('max_write=131072')
        args.append('no_remote_lock')

    def readdir(self, id_, off):
        # FIXME: Do the returned entries acquire a lookup count?
        log.debug('readdir(%d, %d): start', id_, off)
        if off == 0:
            off = -1

        inode = self.inodes[id_]
        if inode.atime < inode.ctime or inode.atime < inode.mtime:
            inode.atime = time.time()

        # The ResultSet is automatically deleted
        # when yield raises GeneratorExit.  
        res = self.db.query("SELECT name_id, name, inode FROM contents_v "
                            'WHERE parent_inode=? AND name_id > ? ORDER BY name_id', (id_, off))
        for (next_, name, cid_) in res:
            yield (name, self.inodes[cid_], next_)

    def getxattr(self, id_, name):
        log.debug('getxattr(%d, %r): start', id_, name)
        # Handle S3QL commands
        if id_ == CTRL_INODE:
            if name == b's3ql_pid?':
                return bytes(os.getpid())

            elif name == b's3qlstat':
                return self.extstat()

#Added by Jiahong Wu: return current cache status
            elif name == b's3qlcache':
                return self.extstat_cache()

            raise llfuse.FUSEError(errno.EINVAL)

        else:
            try:
                value = self.db.get_val('SELECT value FROM ext_attributes_v WHERE inode=? AND name=?',
                                          (id_, name))
            except NoSuchRowError:
                raise llfuse.FUSEError(llfuse.ENOATTR)
            return value

    def listxattr(self, id_):
        log.debug('listxattr(%d): start', id_)
        names = list()
        for (name,) in self.db.query('SELECT name FROM ext_attributes_v WHERE inode=?', (id_,)):
            names.append(name)
        return names

    def setxattr(self, id_, name, value):
        log.debug('setxattr(%d, %r, %r): start', id_, name, value)

        # Handle S3QL commands
        if id_ == CTRL_INODE:
            if name == b's3ql_flushcache!':
                self.cache.clear()
            elif name == 'uploadon':
                self.cache.do_upload = True
            elif name == 'uploadoff':
                self.cache.do_upload = False
            elif name == 'writeon':
                self.cache.do_write = True
            elif name == 'writeoff':
                self.cache.do_write = False
            elif name == 'copy':
                self.copy_tree(*pickle.loads(value))
            elif name == 'upload-meta':
                if self.upload_event is not None:
                    self.upload_event.set()
                else:
                    raise llfuse.FUSEError(errno.ENOTTY)
            elif name == 'lock':
                self.lock_tree(*pickle.loads(value))
            elif name == 'unlock':
                self.unlock_tree(*pickle.loads(value))
            elif name == 'rmtree':
                self.remove_tree(*pickle.loads(value))
            elif name == 'logging':
                update_logging(*pickle.loads(value))
            elif name == 'cachesize':
                self.cache.max_size = pickle.loads(value)
            else:
                raise llfuse.FUSEError(errno.EINVAL)
        else:
            if not self.cache.do_write:
                raise FUSEError(errno.ENOSPC)
            
            if self.inodes[id_].locked:
                raise FUSEError(errno.EPERM)

            if len(value) > deltadump.MAX_BLOB_SIZE:
                raise FUSEError(errno.EINVAL)

            self.db.execute('INSERT OR REPLACE INTO ext_attributes (inode, name_id, value) '
                            'VALUES(?, ?, ?)', (id_, self._add_name(name), value))
            self.inodes[id_].ctime = time.time()

    def removexattr(self, id_, name):
        log.debug('removexattr(%d, %r): start', id_, name)

        if not self.cache.do_write:
            raise FUSEError(errno.ENOSPC)
            
        if self.inodes[id_].locked:
            raise FUSEError(errno.EPERM)

        try:
            name_id = self._del_name(name)
        except NoSuchRowError:
            raise llfuse.FUSEError(llfuse.ENOATTR)

        changes = self.db.execute('DELETE FROM ext_attributes WHERE inode=? AND name_id=?',
                                  (id_, name_id))
        if changes == 0:
            raise llfuse.FUSEError(llfuse.ENOATTR)

        self.inodes[id_].ctime = time.time()

    def lock_tree(self, id0):
        '''Lock directory tree'''

        log.debug('lock_tree(%d): start', id0)
        queue = [ id0 ]
        self.inodes[id0].locked = True
        processed = 0 # Number of steps since last GIL release
        stamp = time.time() # Time of last GIL release
        gil_step = 250 # Approx. number of steps between GIL releases
        while True:
            id_p = queue.pop()
            for (id_,) in self.db.query('SELECT inode FROM contents WHERE parent_inode=?',
                                        (id_p,)):
                self.inodes[id_].locked = True
                processed += 1

                if self.db.has_val('SELECT 1 FROM contents WHERE parent_inode=?', (id_,)):
                    queue.append(id_)

            if not queue:
                break

            if processed > gil_step:
                dt = time.time() - stamp
                gil_step = max(int(gil_step * GIL_RELEASE_INTERVAL / dt), 250)
                log.debug('lock_tree(%d): Adjusting gil_step to %d',
                          id0, gil_step)
                processed = 0
                llfuse.lock.yield_(100)
                log.debug('lock_tree(%d): re-acquired lock', id0)
                stamp = time.time()

        log.debug('lock_tree(%d): end', id0)
    
    # wthung, add unlock
    def unlock_tree(self, id0):
        '''Lock directory tree'''

        log.debug('unlock_tree(%d): start', id0)
        queue = [ id0 ]
        self.inodes[id0].locked = False
        processed = 0 # Number of steps since last GIL release
        stamp = time.time() # Time of last GIL release
        gil_step = 250 # Approx. number of steps between GIL releases
        while True:
            id_p = queue.pop()
            for (id_,) in self.db.query('SELECT inode FROM contents WHERE parent_inode=?',
                                        (id_p,)):
                self.inodes[id_].locked = False
                processed += 1

                if self.db.has_val('SELECT 1 FROM contents WHERE parent_inode=?', (id_,)):
                    queue.append(id_)

            if not queue:
                break

            if processed > gil_step:
                dt = time.time() - stamp
                gil_step = max(int(gil_step * GIL_RELEASE_INTERVAL / dt), 250)
                log.debug('unlock_tree(%d): Adjusting gil_step to %d',
                          id0, gil_step)
                processed = 0
                llfuse.lock.yield_(100)
                log.debug('unlock_tree(%d): re-acquired lock', id0)
                stamp = time.time()

        log.debug('unlock_tree(%d): end', id0)

    def remove_tree(self, id_p0, name0):
        '''Remove directory tree'''

        log.debug('remove_tree(%d, %s): start', id_p0, name0)

        if self.inodes[id_p0].locked:
            raise FUSEError(errno.EPERM)

        id0 = self.lookup(id_p0, name0).id
        queue = [ id0 ]
        processed = 0 # Number of steps since last GIL release
        stamp = time.time() # Time of last GIL release
        gil_step = 250 # Approx. number of steps between GIL releases
        while True:
            found_subdirs = False
            id_p = queue.pop()
            for (name_id, id_) in self.db.query('SELECT name_id, inode FROM contents WHERE '
                                                'parent_inode=?', (id_p,)):

                if self.db.has_val('SELECT 1 FROM contents WHERE parent_inode=?', (id_,)):
                    if not found_subdirs:
                        found_subdirs = True
                        queue.append(id_p)
                    queue.append(id_)

                else:
                    name = self.db.get_val("SELECT name FROM names WHERE id=?", (name_id,))
                    if id_p in self.open_inodes:
                        llfuse.invalidate_entry(id_p, name)
                    self._remove(id_p, name, id_, force=True)

                processed += 1
                if processed > gil_step:
                    if not found_subdirs:
                        found_subdirs = True
                        queue.append(id_p)
                    break

            if not queue:
                if id_p0 in self.open_inodes:
                    llfuse.invalidate_entry(id_p0, name0)
                self._remove(id_p0, name0, id0, force=True)
                break

            if processed > gil_step:
                dt = time.time() - stamp
                gil_step = max(int(gil_step * GIL_RELEASE_INTERVAL / dt), 250)
                log.debug('remove_tree(%d, %s): Adjusting gil_step to %d and yielding',
                          id_p0, name0, gil_step)
                processed = 0
                llfuse.lock.yield_(100)
                log.debug('remove_tree(%d, %s): re-acquired lock', id_p0, name0)
                stamp = time.time()

        self.forget([(id0, 1)])
        log.debug('remove_tree(%d, %s): end', id_p0, name0)


#Jiahong: Snapshotting begins with dirty cache flush, and only after complete flush will metadata replication starts 
    def copy_tree(self, src_id, target_id):
        '''Efficiently copy directory tree'''

        log.debug('copy_tree(%d, %d): start', src_id, target_id)

        # To avoid lookups and make code tidier
        make_inode = self.inodes.create_inode
        db = self.db

        # First we make sure that all blocks are in the database
        #Jiahong: commenting out the following. We do not need to use this function
        #self.cache.commit()
        #log.debug('copy_tree(%d, %d): committed cache', src_id, target_id)

        #Jiahong: A monitoring code here to probe for clean cache and raise EAGAIN error if contain dirty cache
        if self.cache.dirty_entries > 0:
            self.cache.snapshot_upload = True #Start flushing to cloud
            raise FUSEError(errno.EAGAIN)

        # Copy target attributes
        # These come from setxattr, so they may have been deleted
        # without being in open_inodes
        try:
            src_inode = self.inodes[src_id]
            target_inode = self.inodes[target_id]
        except KeyError:
            raise FUSEError(errno.ENOENT)
        for attr in ('atime', 'ctime', 'mtime', 'mode', 'uid', 'gid'):
            setattr(target_inode, attr, getattr(src_inode, attr))

        #Jiahong: record statistics of this snapshotting (number of files, size)
        snapshot_total_files = 0
        snapshot_total_size = 0

        # We first replicate into a dummy inode, so that we
        # need to invalidate only once.
        timestamp = time.time()
        tmp = make_inode(mtime=timestamp, ctime=timestamp, atime=timestamp,
                         uid=0, gid=0, mode=0, refcount=0)

        queue = [ (src_id, tmp.id, 0) ]
        id_cache = dict()
        processed = 0 # Number of steps since last GIL release
        stamp = time.time() # Time of last GIL release
        gil_step = 250 # Approx. number of steps between GIL releases
        while queue:
            (src_id, target_id, off) = queue.pop()
            log.debug('copy_tree(%d, %d): Processing directory (%d, %d, %d)',
                      src_inode.id, target_inode.id, src_id, target_id, off)
            for (name_id, id_) in db.query('SELECT name_id, inode FROM contents '
                                           'WHERE parent_inode=? AND name_id > ? '
                                           'ORDER BY name_id', (src_id, off)):
                if id_ not in id_cache:
                    inode = self.inodes[id_]

                    try:
                        inode_new = make_inode(refcount=1, mode=inode.mode, size=inode.size,
                                               uid=inode.uid, gid=inode.gid,
                                               mtime=inode.mtime, atime=inode.atime,
                                               ctime=inode.ctime, rdev=inode.rdev)
                        # wthung, 2012/10/24
                        # update value cache
                        self.cache.value_cache["inodes"] += 1
                        self.cache.value_cache["fs_size"] += inode.size
                    except OutOfInodesError:
                        log.warn('Could not find a free inode')
                        raise FUSEError(errno.ENOSPC)

                    #Jiahong: updating statistics
                    snapshot_total_files = snapshot_total_files + 1
                    snapshot_total_size = snapshot_total_size + inode.size

                    id_new = inode_new.id

                    if inode.refcount != 1:
                        id_cache[id_] = id_new

                    db.execute('INSERT INTO symlink_targets (inode, target) '
                               'SELECT ?, target FROM symlink_targets WHERE inode=?',
                               (id_new, id_))

                    db.execute('INSERT INTO ext_attributes (inode, name_id, value) '
                               'SELECT ?, name_id, value FROM ext_attributes WHERE inode=?',
                               (id_new, id_))
                    db.execute('UPDATE names SET refcount = refcount + 1 WHERE '
                               'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                               (id_,))

                    processed += db.execute('INSERT INTO inode_blocks (inode, blockno, block_id) '
                                            'SELECT ?, blockno, block_id FROM inode_blocks '
                                            'WHERE inode=?', (id_new, id_))
                    # wthung todo: need to investigate what the following sql command affects
                    db.execute('REPLACE INTO blocks (id, hash, refcount, size, obj_id) '
                               'SELECT id, hash, refcount+COUNT(id), size, obj_id '
                               'FROM inode_blocks JOIN blocks ON block_id = id '
                               'WHERE inode = ? GROUP BY id', (id_new,))

                    if db.has_val('SELECT 1 FROM contents WHERE parent_inode=?', (id_,)):
                        queue.append((id_, id_new, 0))
                else:
                    id_new = id_cache[id_]
                    self.inodes[id_new].refcount += 1

                db.execute('INSERT INTO contents (name_id, inode, parent_inode) VALUES(?, ?, ?)',
                           (name_id, id_new, target_id))
                # wthung, 2012/10/24
                # update value cache
                self.cache.value_cache["entries"] += 1
                db.execute('UPDATE names SET refcount=refcount+1 WHERE id=?', (name_id,))

                processed += 1

#Jiahong: commented out the yielding process for now
                #if processed > gil_step:
                #    log.debug('copy_tree(%d, %d): Requeueing (%d, %d, %d) to yield lock',
                #              src_inode.id, target_inode.id, src_id, target_id, name_id)
                #    queue.append((src_id, target_id, name_id))
                #    break

#Jiahong: commented out the yielding process for now
            #if processed > gil_step:
            #    dt = time.time() - stamp
            #    gil_step = max(int(gil_step * GIL_RELEASE_INTERVAL / dt), 250)
            #    log.debug('copy_tree(%d, %d): Adjusting gil_step to %d and yielding',
            #              src_inode.id, target_inode.id, gil_step)
            #    processed = 0
            #    llfuse.lock.yield_(100)
            #    log.debug('copy_tree(%d, %d): re-acquired lock',
            #              src_inode.id, target_inode.id)
            #    stamp = time.time()

        # Make replication visible
        self.db.execute('UPDATE contents SET parent_inode=? WHERE parent_inode=?',
                        (target_inode.id, tmp.id))
        del self.inodes[tmp.id]
        llfuse.invalidate_inode(target_inode.id)

        #write statistics to /root/.s3ql
        try:
            with open('/root/.s3ql/snapshot.log','w') as fh:
                fh.write('total files: %d\n' % snapshot_total_files)
                fh.write('total size: %d\n' % snapshot_total_size)
        except:
            log.warning('Unable to write snapshot statistics to log. Skipping logging')

        self.cache.snapshot_upload = False

        log.debug('copy_tree(%d, %d): end', src_inode.id, target_inode.id)

    def unlink(self, id_p, name):
        log.debug('rmdir(%d, %r): start', id_p, name)
        inode = self.lookup(id_p, name)

        if stat.S_ISDIR(inode.mode):
            raise llfuse.FUSEError(errno.EISDIR)

        self._remove(id_p, name, inode.id)

        self.forget([(inode.id, 1)])

    def rmdir(self, id_p, name):
        log.debug('rmdir(%d, %r): start', id_p, name)
        inode = self.lookup(id_p, name)

        if self.inodes[id_p].locked:
            raise FUSEError(errno.EPERM)

        if not stat.S_ISDIR(inode.mode):
            raise llfuse.FUSEError(errno.ENOTDIR)

        self._remove(id_p, name, inode.id)

        self.forget([(inode.id, 1)])

    def _remove(self, id_p, name, id_, force=False):
        '''Remove entry `name` with parent inode `id_p` 
        
        `id_` must be the inode of `name`. If `force` is True, then
        the `locked` attribute is ignored.
        
        This method releases the global lock.
        '''

        log.debug('_remove(%d, %s): start', id_p, name)

        timestamp = time.time()

        # Check that there are no child entries
        if self.db.has_val("SELECT 1 FROM contents WHERE parent_inode=?", (id_,)):
            log.debug("Attempted to remove entry with children: %s",
                      get_path(id_p, self.db, name))
            raise llfuse.FUSEError(errno.ENOTEMPTY)

        if self.inodes[id_p].locked and not force:
            raise FUSEError(errno.EPERM)

        name_id = self._del_name(name)
        self.db.execute("DELETE FROM contents WHERE name_id=? AND parent_inode=?",
                        (name_id, id_p))
        
        # wthung, 2012/10/24
        # update value cache
        self.cache.value_cache["entries"] -= 1
        self.cache.value_cache["fs_size"] -= self.inodes[id_].size
        self.cache.value_cache["entries"] = max(self.cache.value_cache["entries"], 0)
        self.cache.value_cache["fs_size"] = max(self.cache.value_cache["fs_size"], 0)

        inode = self.inodes[id_]
        inode.refcount -= 1
        inode.ctime = timestamp

        inode_p = self.inodes[id_p]
        inode_p.mtime = timestamp
        inode_p.ctime = timestamp

        if inode.refcount == 0 and id_ not in self.open_inodes:
            log.debug('_remove(%d, %s): removing from cache', id_p, name)
            self.cache.remove(id_, 0, int(math.ceil(inode.size / self.max_obj_size)))
            # Since the inode is not open, it's not possible that new blocks
            # get created at this point and we can safely delete the inode
            self.db.execute('UPDATE names SET refcount = refcount - 1 WHERE '
                            'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                            (id_,))
            self.db.execute('DELETE FROM names WHERE refcount=0 AND '
                            'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                            (id_,))
            self.db.execute('DELETE FROM ext_attributes WHERE inode=?', (id_,))
            self.db.execute('DELETE FROM symlink_targets WHERE inode=?', (id_,))
            del self.inodes[id_]
            
            # wthung, 2012/10/24
            # update value cache
            self.cache.value_cache["inodes"] -= 1
            self.cache.value_cache["inodes"] = max(self.cache.value_cache["inodes"], 0)

        log.debug('_remove(%d, %s): start', id_p, name)

    def symlink(self, id_p, name, target, ctx):
        log.debug('symlink(%d, %r, %r): start', id_p, name, target)

        mode = (stat.S_IFLNK | stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |
                stat.S_IRGRP | stat.S_IWGRP | stat.S_IXGRP |
                stat.S_IROTH | stat.S_IWOTH | stat.S_IXOTH)

        # Unix semantics require the size of a symlink to be the length
        # of its target. Therefore, we create symlink directory entries
        # with this size. If the kernel ever learns to open and read
        # symlinks directly, it will read the corresponding number of \0
        # bytes.
        inode = self._create(id_p, name, mode, ctx, size=len(target))
        self.db.execute('INSERT INTO symlink_targets (inode, target) VALUES(?,?)',
                        (inode.id, target))
        self.open_inodes[inode.id] += 1
        return inode

    def rename(self, id_p_old, name_old, id_p_new, name_new):
        log.debug('rename(%d, %r, %d, %r): start', id_p_old, name_old, id_p_new, name_new)
        if name_new == CTRL_NAME or name_old == CTRL_NAME:
            log.warn('Attempted to rename s3ql control file (%s -> %s)',
                      get_path(id_p_old, self.db, name_old),
                      get_path(id_p_new, self.db, name_new))
            raise llfuse.FUSEError(errno.EACCES)

        if (self.inodes[id_p_old].locked
            or self.inodes[id_p_new].locked):
            raise FUSEError(errno.EPERM)

        inode_old = self.lookup(id_p_old, name_old)

        try:
            inode_new = self.lookup(id_p_new, name_new)
        except llfuse.FUSEError as exc:
            if exc.errno != errno.ENOENT:
                raise
            else:
                target_exists = False
        else:
            target_exists = True

        if target_exists:
            self._replace(id_p_old, name_old, id_p_new, name_new,
                          inode_old.id, inode_new.id)
            self.forget([(inode_old.id, 1), (inode_new.id, 1)])
        else:
            self._rename(id_p_old, name_old, id_p_new, name_new)
            self.forget([(inode_old.id, 1)])

    def _add_name(self, name):
        '''Get id for *name* and increase refcount
        
        Name is inserted in table if it does not yet exist.
        '''

        try:
            name_id = self.db.get_val('SELECT id FROM names WHERE name=?', (name,))
        except NoSuchRowError:
            name_id = self.db.rowid('INSERT INTO names (name, refcount) VALUES(?,?)',
                                    (name, 1))
        else:
            self.db.execute('UPDATE names SET refcount=refcount+1 WHERE id=?', (name_id,))
        return name_id

    def _del_name(self, name):
        '''Decrease refcount for *name*
        
        Name is removed from table if refcount drops to zero. Returns the
        (possibly former) id of the name.
        '''

        (name_id, refcount) = self.db.get_row('SELECT id, refcount FROM names WHERE name=?', (name,))

        if refcount > 1:
            self.db.execute('UPDATE names SET refcount=refcount-1 WHERE id=?', (name_id,))
        else:
            self.db.execute('DELETE FROM names WHERE id=?', (name_id,))

        return name_id

    def _rename(self, id_p_old, name_old, id_p_new, name_new):
        timestamp = time.time()

        name_id_new = self._add_name(name_new)
        name_id_old = self._del_name(name_old)

        self.db.execute("UPDATE contents SET name_id=?, parent_inode=? WHERE name_id=? "
                        "AND parent_inode=?", (name_id_new, id_p_new,
                                               name_id_old, id_p_old))

        inode_p_old = self.inodes[id_p_old]
        inode_p_new = self.inodes[id_p_new]
        inode_p_old.mtime = timestamp
        inode_p_new.mtime = timestamp
        inode_p_old.ctime = timestamp
        inode_p_new.ctime = timestamp

    def _replace(self, id_p_old, name_old, id_p_new, name_new,
                 id_old, id_new):

        timestamp = time.time()

        if self.db.has_val("SELECT 1 FROM contents WHERE parent_inode=?", (id_new,)):
            log.info("Attempted to overwrite entry with children: %s",
                      get_path(id_p_new, self.db, name_new))
            raise llfuse.FUSEError(errno.EINVAL)

        # Replace target
        name_id_new = self.db.get_val('SELECT id FROM names WHERE name=?', (name_new,))
        self.db.execute("UPDATE contents SET inode=? WHERE name_id=? AND parent_inode=?",
                        (id_old, name_id_new, id_p_new))

        # Delete old name
        name_id_old = self._del_name(name_old)
        self.db.execute('DELETE FROM contents WHERE name_id=? AND parent_inode=?',
                        (name_id_old, id_p_old))
        
        # wthung, 2012/10/24
        # update value cache
        self.cache.value_cache["entries"] -= 1
        self.cache.value_cache["entries"] = max(self.cache.value_cache["entries"], 0)

        inode_new = self.inodes[id_new]
        inode_new.refcount -= 1
        inode_new.ctime = timestamp

        inode_p_old = self.inodes[id_p_old]
        inode_p_old.ctime = timestamp
        inode_p_old.mtime = timestamp

        inode_p_new = self.inodes[id_p_new]
        inode_p_new.ctime = timestamp
        inode_p_new.mtime = timestamp

        if inode_new.refcount == 0 and id_new not in self.open_inodes:
            self.cache.remove(id_new, 0,
                              int(math.ceil(inode_new.size / self.max_obj_size)))
            # Since the inode is not open, it's not possible that new blocks
            # get created at this point and we can safely delete the inode
            self.db.execute('UPDATE names SET refcount = refcount - 1 WHERE '
                            'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                            (id_new,))
            self.db.execute('DELETE FROM names WHERE refcount=0')
            self.db.execute('DELETE FROM ext_attributes WHERE inode=?', (id_new,))
            self.db.execute('DELETE FROM symlink_targets WHERE inode=?', (id_new,))
            del self.inodes[id_new]
            
            # wthung, 2012/10/24
            # update value cache
            self.cache.value_cache["inodes"] -= 1
            self.cache.value_cache["inodes"] = max(self.cache.value_cache["inodes"], 0)


    def link(self, id_, new_id_p, new_name):
        log.debug('link(%d, %d, %r): start', id_, new_id_p, new_name)

        if new_name == CTRL_NAME or id_ == CTRL_INODE:
            log.warn('Attempted to create s3ql control file at %s',
                      get_path(new_id_p, self.db, new_name))
            raise llfuse.FUSEError(errno.EACCES)

        timestamp = time.time()
        inode_p = self.inodes[new_id_p]

        if inode_p.refcount == 0:
            log.warn('Attempted to create entry %s with unlinked parent %d',
                     new_name, new_id_p)
            raise FUSEError(errno.EINVAL)

        if not self.cache.do_write:
            raise FUSEError(errno.ENOSPC)
            
        if inode_p.locked:
            raise FUSEError(errno.EPERM)

        inode_p.ctime = timestamp
        inode_p.mtime = timestamp

        self.db.execute("INSERT INTO contents (name_id, inode, parent_inode) VALUES(?,?,?)",
                        (self._add_name(new_name), id_, new_id_p))
        # wthung, 2012/10/24
        # update value cache
        self.cache.value_cache["entries"] += 1
        self.cache.value_cache["fs_size"] += self.inodes[id_].size
        log.debug('link, fs size +%d' % self.inodes[id_].size)
        
        inode = self.inodes[id_]
        inode.refcount += 1
        inode.ctime = timestamp

        self.open_inodes[inode.id] += 1
        return inode

    def setattr(self, id_, attr):
        """Handles FUSE setattr() requests"""
        if log.isEnabledFor(logging.DEBUG):
            log.debug('setattr(%d, %s): start', id_,
                      [ getattr(attr, x) for x in attr.__slots__
                       if getattr(attr, x) is not None ])

        inode = self.inodes[id_]
        timestamp = time.time()

        if not self.cache.do_write:
            raise FUSEError(errno.ENOSPC)
        
        if inode.locked:
            raise FUSEError(errno.EPERM)

        if attr.st_size is not None:
            len_ = attr.st_size

            # Determine blocks to delete
            last_block = len_ // self.max_obj_size
            cutoff = len_ % self.max_obj_size
            total_blocks = int(math.ceil(inode.size / self.max_obj_size))

            # Adjust file size
            inode.size = len_

            # Delete blocks and truncate last one if required 
            if cutoff == 0:
                self.cache.remove(id_, last_block, total_blocks)
            else:
                self.cache.remove(id_, last_block + 1, total_blocks)
                try:
                    with self.cache.get(id_, last_block) as fh:
                        fh.truncate(cutoff)
                except NoSuchObject as exc:
                    log.warn('Backend lost block %d of inode %d (id %s)!',
                             last_block, id_, exc.key)
                    raise

                except ChecksumError as exc:
                    log.warn('Backend returned malformed data for block %d of inode %d (%s)',
                             last_block, id_, exc)
                    raise

        if attr.st_mode is not None:
            inode.mode = attr.st_mode

        if attr.st_uid is not None:
            inode.uid = attr.st_uid

        if attr.st_gid is not None:
            inode.gid = attr.st_gid

        if attr.st_rdev is not None:
            inode.rdev = attr.st_rdev

        if attr.st_atime is not None:
            inode.atime = attr.st_atime

        if attr.st_mtime is not None:
            inode.mtime = attr.st_mtime

        if attr.st_ctime is not None:
            inode.ctime = attr.st_ctime
        else:
            inode.ctime = timestamp

        return inode

    def mknod(self, id_p, name, mode, rdev, ctx):
        log.debug('mknod(%d, %r): start', id_p, name)
        inode = self._create(id_p, name, mode, ctx, rdev=rdev)
        self.open_inodes[inode.id] += 1
        return inode

    def mkdir(self, id_p, name, mode, ctx):
        log.debug('mkdir(%d, %r): start', id_p, name)
        inode = self._create(id_p, name, mode, ctx)
        self.open_inodes[inode.id] += 1
        return inode

    def extstat(self):
        '''Return extended file system statistics'''

        log.debug('extstat(%d): start')

        # Flush inode cache to get better estimate of total fs size
        self.inodes.flush()

        # wthung, 2012/10/24
        # Use refresh count to control the source of data
        self.cache.refresh_count += 1
        if self.cache.refresh_count > REFRESH_COUNT:
            entries = self.db.get_val("SELECT COUNT(rowid) FROM contents")
            blocks = self.db.get_val("SELECT COUNT(id) FROM objects")
            inodes = self.db.get_val("SELECT COUNT(id) FROM inodes")
            fs_size = self.db.get_val('SELECT SUM(size) FROM inodes') or 0
            dedup_size = self.db.get_val('SELECT SUM(size) FROM blocks') or 0
            compr_size = self.db.get_val('SELECT SUM(size) FROM objects') or 0

            # update to value cache
            self.cache.value_cache["entries"] = entries
            self.cache.value_cache["blocks"] = blocks
            self.cache.value_cache["inodes"] = inodes
            self.cache.value_cache["fs_size"] = fs_size
            self.cache.value_cache["dedup_size"] = dedup_size
            self.cache.value_cache["compr_size"] = compr_size

            self.cache.refresh_count = 0
        else:
            entries = self.cache.value_cache["entries"]
            blocks = self.cache.value_cache["blocks"]
            inodes = self.cache.value_cache["inodes"]
            fs_size = self.cache.value_cache["fs_size"]
            dedup_size = self.cache.value_cache["dedup_size"]
            compr_size = self.cache.value_cache["compr_size"]
        
        entries = max(entries,0)
        blocks = max(blocks,0)
        inodes = max(inodes,0)
        fs_size = max(fs_size,0)
        dedup_size = max(dedup_size,0)
        compr_size = max(compr_size,0)

        return struct.pack('QQQQQQQ', entries, blocks, inodes, fs_size, dedup_size,
                           compr_size, self.db.get_size())

#Added by Jiahong Wu: New function that returns status of local cache
    def extstat_cache(self):
        '''Return local cache statistics'''

        log.debug('extstat_cache(%d): start')

        #return additional information re cache
        cache_size = max(self.cache.size,0)
        cache_dirtysize = max(self.cache.dirty_size,0)
        cache_entries = max(len(self.cache.entries),0)
        cache_dirtyentries = max(self.cache.dirty_entries,0)
        cache_maxsize = max(self.cache.max_size,0)
        cache_maxentries = max(self.cache.max_entries,0)
        if (self.cache.do_upload or self.cache.forced_upload or self.cache.snapshot_upload) and self.cache.dirty_size>0:
            cache_uploading = 1
        else:
            cache_uploading = 0
        
        # wthung, 2012/10/15
        # pack a value to show do_write flag
        # this is not a perfect location
        if (self.cache.do_write):
            filesys_write = 1
        else:
            filesys_write = 0

        return struct.pack('QQQQQQQQ', 
                           cache_size, cache_dirtysize, cache_entries, cache_dirtyentries, cache_maxsize, cache_maxentries, cache_uploading, filesys_write)

    def statfs(self):
        log.debug('statfs(): start')

        stat_ = llfuse.StatvfsData

        # Get number of blocks & inodes
        # wthung, 2012/10/24
        # Use refresh count to control the source of data
        self.cache.refresh_count += 1
        if self.cache.refresh_count > REFRESH_COUNT:
            blocks = self.db.get_val("SELECT COUNT(id) FROM objects")
            inodes = self.db.get_val("SELECT COUNT(id) FROM inodes")
            size = self.db.get_val('SELECT SUM(size) FROM blocks')

            # update value cache
            self.cache.value_cache["blocks"] = blocks
            self.cache.value_cache["inodes"] = inodes
            self.cache.value_cache["dedup_size"] = size

            self.cache.refresh_count = 0
        else:
            blocks = self.cache.value_cache["blocks"]
            inodes = self.cache.value_cache["inodes"]
            size = self.cache.value_cache["dedup_size"]

        if size is None:
            size = 0

        # wthung, 2012/11/12, retrieve db value in negative value
        if blocks < 0:
            blocks = self.db.get_val("SELECT COUNT(id) FROM objects")
            self.cache.value_cache["blocks"] = blocks

        if inodes < 0:
            inodes = self.db.get_val("SELECT COUNT(id) FROM inodes")
            self.cache.value_cache["inodes"] = inodes

        if size < 0:
            size = self.db.get_val('SELECT SUM(size) FROM blocks')
            self.cache.value_cache["dedup_size"] = size

        # file system block size, i.e. the minimum amount of space that can
        # be allocated. This doesn't make much sense for S3QL, so we just
        # return the average size of stored blocks.
        stat_.f_frsize = size // blocks if blocks != 0 else 4096
        if stat_.f_frsize == 0:  # Jiahong: Extra error handling for ensuring that the f_frsize var won't be zero
            stat_.f_frsize = 4096

        # This should actually be the "preferred block size for doing IO.  However, `df` incorrectly
        # interprets f_blocks, f_bfree and f_bavail in terms of f_bsize rather than f_frsize as it
        # should (according to statvfs(3)), so the only way to return correct values *and* have df
        # print something sensible is to set f_bsize and f_frsize to the same value. (cf.
        # http://bugs.debian.org/671490)
        stat_.f_bsize = stat_.f_frsize

        # size of fs in f_frsize units. Since backend is supposed to be unlimited,
        # always return a half-full filesystem, but at least 1 TB)
        fs_size = max(2 * size, 1024 ** 4)

        stat_.f_blocks = fs_size // stat_.f_frsize
        stat_.f_bfree = (fs_size - size) // stat_.f_frsize
        stat_.f_bavail = stat_.f_bfree # free for non-root

        total_inodes = max(2 * inodes, 1000000)
        stat_.f_files = total_inodes
        stat_.f_ffree = total_inodes - inodes
        stat_.f_favail = total_inodes - inodes # free for non-root

        return stat_

    def open(self, id_, flags):
        log.debug('open(%d): start', id_)        
        
        if ((flags & os.O_RDWR or flags & os.O_WRONLY)
            and self.inodes[id_].locked):
            raise FUSEError(errno.EPERM)

        return id_

    def access(self, id_, mode, ctx):
        '''Check if requesting process has `mode` rights on `inode`.
        
        This method always returns true, since it should only be called
        when permission checking is disabled (if permission checking is
        enabled, the `default_permissions` FUSE option should be set).
        '''
        # Yeah, could be a function and has unused arguments
        #pylint: disable=R0201,W0613

        log.debug('access(%d): executed', id_)
        return True

    def create(self, id_p, name, mode, flags, ctx):
        log.debug('create(id_p=%d, %s): started', id_p, name)
        try:
            id_ = self.db.get_val("SELECT inode FROM contents_v WHERE name=? AND parent_inode=?",
                                  (name, id_p))
        except NoSuchRowError:
            inode = self._create(id_p, name, mode, ctx)
        else:
            self.open(id_, flags)
            inode = self.inodes[id_]

        self.open_inodes[inode.id] += 1
        return (inode.id, inode)

    def _create(self, id_p, name, mode, ctx, rdev=0, size=0):
        if name == CTRL_NAME:
            log.warn('Attempted to create s3ql control file at %s',
                     get_path(id_p, self.db, name))
            raise llfuse.FUSEError(errno.EACCES)

        timestamp = time.time()
        inode_p = self.inodes[id_p]

        if not self.cache.do_write:
            raise FUSEError(errno.ENOSPC)
            
        if inode_p.locked:
            raise FUSEError(errno.EPERM)

        if inode_p.refcount == 0:
            log.warn('Attempted to create entry %s with unlinked parent %d',
                     name, id_p)
            raise FUSEError(errno.EINVAL)
        inode_p.mtime = timestamp
        inode_p.ctime = timestamp

        try:
            inode = self.inodes.create_inode(mtime=timestamp, ctime=timestamp, atime=timestamp,
                                             uid=ctx.uid, gid=ctx.gid, mode=mode, refcount=1,
                                             rdev=rdev, size=size)
            # wthung, 2012/10/24
            # update value cache
            self.cache.value_cache["inodes"] += 1
            self.cache.value_cache["fs_size"] += size
            log.debug("fs size +%d" % size)
        except OutOfInodesError:
            log.warn('Could not find a free inode')
            raise FUSEError(errno.ENOSPC)

        self.db.execute("INSERT INTO contents(name_id, inode, parent_inode) VALUES(?,?,?)",
                        (self._add_name(name), inode.id, id_p))
        # wthung, 2012/10/24
        # update value cache
        self.cache.value_cache["entries"] += 1

        return inode


    def read(self, fh, offset, length):
        '''Read `size` bytes from `fh` at position `off`
        
        Unless EOF is reached, returns exactly `size` bytes. 
        
        This method releases the global lock while it is running.
        '''
        log.debug('read(%d, %d, %d): start', fh, offset, length)

        buf = StringIO()
        inode = self.inodes[fh]

        # Make sure that we don't read beyond the file size. This
        # should not happen unless direct_io is activated, but it's
        # cheap and nice for testing.
        size = inode.size
        length = min(size - offset, length)

        while length > 0:
            tmp = self._read(fh, offset, length)
            buf.write(tmp)
            length -= len(tmp)
            offset += len(tmp)

        # Inode may have expired from cache 
        inode = self.inodes[fh]

        if inode.atime < inode.ctime or inode.atime < inode.mtime:
            inode.atime = time.time()

        return buf.getvalue()

    def _read(self, id_, offset, length):
        """Reads at the specified position until the end of the block

        This method may return less than `length` bytes if a max_obj_size
        boundary is encountered. It may also read beyond the end of
        the file, filling the buffer with additional null bytes.
        
        This method releases the global lock while it is running.
        """

        # Calculate required block
        blockno = offset // self.max_obj_size
        offset_rel = offset - blockno * self.max_obj_size

        # Don't try to read into the next block
        if offset_rel + length > self.max_obj_size:
            length = self.max_obj_size - offset_rel

        try:
            with self.cache.get(id_, blockno) as fh:
                fh.seek(offset_rel)
                buf = fh.read(length)

        except NoSuchObject as exc:
            log.warn('Backend lost block %d of inode %d (id %s)!',
                     blockno, id_, exc.key)
            raise

        except ChecksumError as exc:
            log.warn('Backend returned malformed data for block %d of inode %d (%s)',
                     blockno, id_, exc)
            raise

        if len(buf) == length:
            return buf
        else:
            # If we can't read enough, add null bytes
            return buf + b"\0" * (length - len(buf))

# Jiahong: TODO: Block write to FUSE when network connection to backend is down and dirty cache occupied all cache  
    def write(self, fh, offset, buf):
        '''Handle FUSE write requests.
        
        This method releases the global lock while it is running.
        '''
        log.debug('write(%d, %d, datalen=%d): start', fh, offset, len(buf))

        if not self.cache.do_write:
            raise FUSEError(errno.ENOSPC)

        if self.inodes[fh].locked:
            raise FUSEError(errno.EPERM)

        total = len(buf)
        minsize = offset + total
        while buf:
            written = self._write(fh, offset, buf)
            # wthung, 2012/10/24
            # update value cache
            self.cache.value_cache["fs_size"] += written
            offset += written
            buf = buf[written:]

        # Update file size if changed
        # Fuse does not ensure that we do not get concurrent write requests,
        # so we have to be careful not to undo a size extension made by
        # a concurrent write.
        timestamp = time.time()
        inode = self.inodes[fh]
        inode.size = max(inode.size, minsize)
        inode.mtime = timestamp
        inode.ctime = timestamp

        return total


    def _write(self, id_, offset, buf):
        """Write as much as we can.

        May write less bytes than given in `buf`, returns
        the number of bytes written.
        
        This method releases the global lock while it is running.
        """

        # Calculate required block
        blockno = offset // self.max_obj_size
        offset_rel = offset - blockno * self.max_obj_size

        # Don't try to write into the next block
        if offset_rel + len(buf) > self.max_obj_size:
            buf = buf[:self.max_obj_size - offset_rel]

        try:
            with self.cache.get(id_, blockno) as fh:
                fh.seek(offset_rel)
                fh.write(buf)

        except NoSuchObject as exc:
            log.warn('Backend lost block %d of inode %d (id %s)!',
                     blockno, id_, exc.key)
            raise

        except ChecksumError as exc:
            log.warn('Backend returned malformed data for block %d of inode %d (%s)',
                     blockno, id_, exc)
            raise

        return len(buf)

    def fsync(self, fh, datasync):
        log.debug('fsync(%d, %s): start', fh, datasync)
        if not datasync:
            self.inodes.flush_id(fh)

        self.cache.flush(fh)

    def forget(self, forget_list):
        log.debug('forget(%s): start', forget_list)

        for (id_, nlookup) in forget_list:
            self.open_inodes[id_] -= nlookup

            if self.open_inodes[id_] == 0:
                del self.open_inodes[id_]

                inode = self.inodes[id_]
                if inode.refcount == 0:
                    log.debug('_forget(%s): removing %d from cache', forget_list, id_)
                    self.cache.remove(id_, 0, int(math.ceil(inode.size / self.max_obj_size)))
                    # Since the inode is not open, it's not possible that new blocks
                    # get created at this point and we can safely delete the inode
                    self.db.execute('UPDATE names SET refcount = refcount - 1 WHERE '
                                    'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                                    (id_,))
                    self.db.execute('DELETE FROM names WHERE refcount=0 AND '
                                    'id IN (SELECT name_id FROM ext_attributes WHERE inode=?)',
                                    (id_,))
                    self.db.execute('DELETE FROM ext_attributes WHERE inode=?', (id_,))
                    self.db.execute('DELETE FROM symlink_targets WHERE inode=?', (id_,))
                    del self.inodes[id_]
                    
                    # wthung, 2012/10/24
                    # update value cache
                    self.cache.value_cache["inodes"] -= 1


    def fsyncdir(self, fh, datasync):
        log.debug('fsyncdir(%d, %s): start', fh, datasync)
        if not datasync:
            self.inodes.flush_id(fh)

    def releasedir(self, fh):
        log.debug('releasedir(%d): start', fh)

    def release(self, fh):
        log.debug('release(%d): start', fh)

    def flush(self, fh):
        log.debug('flush(%d): start', fh)

def update_logging(level, modules):
    root_logger = logging.getLogger()
    if level == logging.DEBUG:
        logging.disable(logging.NOTSET)
        for handler in root_logger.handlers:
            for filter_ in [ f for f in handler.filters if isinstance(f, LoggerFilter) ]:
                handler.removeFilter(filter_)
            handler.setLevel(level)
        if 'all' not in modules:
            for handler in root_logger.handlers:
                handler.addFilter(LoggerFilter(modules, logging.INFO))

    else:
        logging.disable(logging.DEBUG)
    root_logger.setLevel(level)
