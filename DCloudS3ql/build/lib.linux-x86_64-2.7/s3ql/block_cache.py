'''
block_cache.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) 2008-2009 Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
Jia-Hong marking this file on 4/16/2012 for future edits
'''

from __future__ import division, print_function, absolute_import
from .common import sha256_fh, BUFSIZE, QuietError
from .database import NoSuchRowError
from .ordered_dict import OrderedDict
from Queue import Queue
from contextlib import contextmanager
from llfuse import lock, lock_released, FUSEError
import llfuse
import errno
import logging
import os
import shutil
import threading
import time
import stat
import re
from .backends.common import NoSuchObject

# standard logger for this module
log = logging.getLogger("BlockCache")

# Special queue entry that signals threads to terminate
QuitSentinel = object()

class Distributor(object):
    '''
    Distributes objects to consumers.
    '''

    def __init__(self):
        super(Distributor, self).__init__()

        self.slot = None
        self.cv = threading.Condition()

    def put(self, obj):
        '''Offer *obj* for consumption
        
        The method blocks until another thread calls `get()` to consume
        the object.
        '''

        if obj is None:
            raise ValueError("Can't put None into Queue")

        with self.cv:
            while self.slot is not None:
                self.cv.wait()
            self.slot = obj
            self.cv.notify_all()

    def get(self):
        '''Consume and return an object
        
        The method blocks until another thread offers an object
        by calling the `put` method.
        '''
        with self.cv:
            while not self.slot:
                self.cv.wait()
            tmp = self.slot
            self.slot = None
            self.cv.notify_all()
        return tmp


class SimpleEvent(object):
    '''
    Like threading.Event, but without any internal flag. Calls
    to `wait` always block until some other thread calls
    `notify` or `notify_all`.
    '''

    def __init__(self):
        super(SimpleEvent, self).__init__()
        self.__cond = threading.Condition(threading.Lock())

    def notify_all(self):
        self.__cond.acquire()
        try:
            self.__cond.notify_all()
        finally:
            self.__cond.release()

    def notify(self):
        self.__cond.acquire()
        try:
            self.__cond.notify()
        finally:
            self.__cond.release()

    def wait(self):
        self.__cond.acquire()
        try:
            self.__cond.wait()
            return
        finally:
            self.__cond.release()


# Modified by Jia-Hong on 4/16/2012: If the cache file access mode is RW, then it is dirty. If R-only, then clean.
class CacheEntry(object):
    """An element in the block cache
    
    If `obj_id` is `None`, then the object has not yet been
    uploaded to the backend. 
    
    Attributes:
    -----------
    
    :dirty:
       entry has been changed since it was last uploaded.
    
    :size: current file size
    
    :pos: current position in file
    """

    __slots__ = [ 'dirty', 'inode', 'blockno', 'last_upload',
                  'size', 'pos', 'fh' ]

#Jiahong: modified this function and allow the creation of cache entry from existing cache file
    def __init__(self, inode, blockno, filename, newfile=True):
        super(CacheEntry, self).__init__()
        # Writing 100MB in 128k chunks takes 90ms unbuffered and
        # 116ms with 1 MB buffer. Reading time does not depend on
        # buffer size.
        if newfile:
            self.fh = open(filename, "w+b", 0)
            self.dirty = False
        else:
            mode_bits=stat.S_IMODE(os.stat(filename).st_mode)
            if mode_bits & 128:
                self.fh = open(filename, "r+b", 0)
                self.fh.seek(0)
                os.fchmod(self.fh.fileno(), stat.S_IRUSR | stat.S_IWUSR)
                self.dirty = True
            else:
                self.fh = open(filename, "r+b", 0)
                self.fh.seek(0)
                os.fchmod(self.fh.fileno(), stat.S_IRUSR)
                self.dirty = False

        self.inode = inode
        self.blockno = blockno
        self.last_upload = time.time()
        self.pos = 0
        self.size = os.fstat(self.fh.fileno()).st_size

    def read(self, size=None):
        buf = self.fh.read(size)
        self.pos += len(buf)
        return buf

    def flush(self):
        self.fh.flush()

    def seek(self, off):
        if self.pos != off:
            self.fh.seek(off)
            self.pos = off

    def tell(self):
        return self.pos

    def truncate(self, size=None):
        if self.dirty == False:
            os.fchmod(self.fh.fileno(), stat.S_IRUSR | stat.S_IWUSR)
            self.dirty = True
        self.fh.truncate(size)
        if size is None:
            if self.pos < self.size:
                self.size = self.pos
        elif size < self.size:
            self.size = size

    def write(self, buf):
        if self.dirty == False:
            os.fchmod(self.fh.fileno(), stat.S_IRUSR | stat.S_IWUSR)
            self.dirty = True
        self.fh.write(buf)
        self.pos += len(buf)
        self.size = max(self.pos, self.size)

    def close(self):
        self.fh.close()

    def unlink(self):
        os.unlink(self.fh.name)

    #Jiahong:New function for checking if the cache blocks are partially downloaded
    #Jiahong:If the group read bit of the cache files is on, then it is still being downloaded
    def download_set(self):
        os.fchmod(self.fh.fileno(), stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP) 
        self.dirty = True

    def __str__(self):
        return ('<%sCacheEntry, inode=%d, blockno=%d>'
                % ('Dirty ' if self.dirty else '', self.inode, self.blockno))

class BlockCache(object):
    """Provides access to file blocks
    
    This class manages access to file blocks. It takes care of creation,
    uploading, downloading and deduplication.
 
    This class uses the llfuse global lock. Methods which release the lock have
    are marked as such in their docstring.
    
    Attributes
    ----------
    
    :path: where cached data is stored
    :entries: ordered dictionary of cache entries
    :size: current size of all cached entries
    :max_size: maximum size to which cache can grow
    :in_transit: set of objects currently in transit and
         (inode, blockno) tuples currently being uploaded 
    :removed_in_transit: set of objects that have been removed from the db
       while in transit, and should be removed from the backend as soon
       as the transit completes.
    :to_upload: distributes objects to upload to worker threads
    :to_remove: distributes objects to remove to worker threads
    :transfer_complete: signals completion of an object transfer
       (either upload or download)
    
    The `in_transit` attribute is used to
    - Prevent multiple threads from downloading the same object
    - Prevent threads from downloading an object before it has been
      uploaded completely (can happen when a cache entry is linked to a
      block while the object containing the block is still being
      uploaded)
    """

    def __init__(self, bucket_pool, db, cachedir, max_size, max_entries=768):
        log.debug('Initializing')

        self.path = cachedir
        self.db = db
        self.bucket_pool = bucket_pool
        self.entries = OrderedDict()
        self.max_entries = max_entries
        self.size = 0
        self.dirty_size = 0
        self.dirty_entries = 0
        self.max_size = max_size
        self.in_transit = set()
        self.removed_in_transit = set()
        self.to_upload = Distributor()
        self.to_remove = Queue(250)
        self.upload_threads = []
        self.removal_threads = []
        self.transfer_completed = SimpleEvent()
        self.preload_cache = False
        self.do_upload = False
        self.forced_upload = False
        #Jiahong: Adding a mechanism to monitor alive upload threads
        self.last_checked = time.time()
        self.going_down = False #Jiahong: New switch on knowing when the cache will be destroyed
        self.thread_checking = False #Jiahong: New flag for knowing if the upload thread respawning check is in progress


        if os.access(self.path,os.F_OK):
            pass
        else:
            os.mkdir(self.path)

    def __len__(self):
        '''Get number of objects in cache'''
        return len(self.entries)

    def read_cachefiles(self):
        '''Read cache files as initial cache entries'''

#Changed the initialization steps of block caches to read in existing block cache files when the gateway is started / remounted
#steps: scan the cache dir then get() all (inode,block) that are in the cache dir, using modified get()

        if os.access(self.path,os.F_OK) and self.preload_cache == False:
            self.preload_cache = True
            initial_cache_list=os.listdir(self.path)
            for cache_files in initial_cache_list:

                match = re.match('^(\\d+)-(\\d+)$', cache_files)
                if match:
                    tmp_inode = int(match.group(1))
                    tmp_block = int(match.group(2))
                else:
                    raise RuntimeError('Strange file in cache directory: %s' % cache_files)

                with self.get(tmp_inode,tmp_block) as fh:
                    pass


    def init(self, threads=1):
        '''Start worker threads'''

        for _ in range(threads):
            t = threading.Thread(target=self._upload_loop)
            t.start()
            self.upload_threads.append(t)

        for _ in range(10):
            t = threading.Thread(target=self._removal_loop)
            t.daemon = True # interruption will do no permanent harm
            t.start()
            self.removal_threads.append(t)

    #Jiahong Wu (5/7/12): New function for monitoring upload threads
    def check_alive_threads(self):
        '''Monitor if the upload threads are alive. Restart them if necessary'''

        self.thread_checking = True
        finished = False
        self.last_checked = time.time()

        while (not finished) and (not self.going_down):
            finished = True
            for t in self.upload_threads:
                if self.going_down:
                    break
                if not t.isAlive():
                    log.warning('A upload thread has died. Restarting.')
                    t.join
                    t = threading.Thread(target=self._upload_loop)
                    t.start()
                    self.upload_threads.append(t)
                    finished = False
                    break

        self.thread_checking = False


    def destroy(self):
        '''Clean up and stop worker threads'''

        self.going_down = True

        time.sleep(2) #Checking if thread spawning check is in progress
        while self.thread_checking:
            time.sleep(1)

        with lock_released:
            for t in self.upload_threads:
                self.to_upload.put(QuitSentinel)

            for t in self.removal_threads:
                self.to_remove.put(QuitSentinel)

            log.debug('destroy(): waiting for upload threads...')
            for t in self.upload_threads:
                t.join()

            log.debug('destroy(): waiting for removal threads...')
            for t in self.removal_threads:
                t.join()

        self.upload_threads = []
        self.removal_threads = []

        #Jiahong: close opened cache files here, after all uploads are completed
        log.debug('destroy(): close opened cache...')
        self.close_cache()

        #Jiahong: Commented out the following line (we always want the cache directory)
        #os.rmdir(self.path) 

    def _upload_loop(self):
        '''Process upload queue'''

        while True:
            tmp = self.to_upload.get()

            if tmp is QuitSentinel:
                break

            self._do_upload(*tmp)

    def _do_upload(self, el, obj_id):
        '''Upload object'''

        def do_write(fh):
            el.seek(0)
            while True:
                buf = el.read(BUFSIZE)
                if not buf:
                    break
                fh.write(buf)
            return fh

        try:
            if log.isEnabledFor(logging.DEBUG):
                time_ = time.time()
            
            #Jiahong Wu (5/8/12): Added retry mechanism for cache upload. Will retry until system umount.
            while True:
                try:
                    with self.bucket_pool() as bucket:
                        obj_size = bucket.perform_write(do_write, 's3ql_data_%d' % obj_id).get_obj_size()
                    break
                except:
                    if self.going_down:
                        raise
                    log.error('Cache upload timed out. Retrying in 10 seconds.')
                    time.sleep(10)
                    while True:
                        try:
                            with self.bucket_pool() as bucket:
                                bucket.bucket.conn.close()
                                bucket.bucket.conn = bucket.bucket._get_conn()
                            break
                        except:
                            if self.going_down:
                                raise
                            log.error('Network may be disconnected. Retrying in 10 seconds.')
                            time.sleep(10)
                    

            if log.isEnabledFor(logging.DEBUG):
                time_ = time.time() - time_
                rate = el.size / (1024 ** 2 * time_) if time_ != 0 else 0
                log.debug('_do_upload(%s): uploaded %d bytes in %.3f seconds, %.2f MB/s',
                          obj_id, el.size, time_, rate)

            with lock:
                self.db.execute('UPDATE objects SET size=? WHERE id=?',
                                (obj_size, obj_id))
                os.fchmod(el.fh.fileno(), stat.S_IRUSR)
                #Update dirty cache size/entries and stop forced uploading if dirty cache size/entries drop below some limit
                if el.dirty:
                    self.dirty_size -= el.size
                    self.dirty_entries -= 1
                    if self.dirty_size < 0:
                        self.dirty_size = 0 
                    if self.dirty_entries < 0:
                        self.dirty_entries = 0
                    if self.dirty_size < 0.5*self.max_size and self.dirty_entries < 0.5*self.max_entries:
                        self.forced_upload = False
                el.dirty = False
                el.last_upload = time.time()
                self.in_transit.remove(obj_id)
                self.in_transit.remove((el.inode, el.blockno))
                self.transfer_completed.notify_all()

        except Exception as exc:
            with lock:
                self.in_transit.remove(obj_id)
                self.in_transit.remove((el.inode, el.blockno))
                self.transfer_completed.notify_all()

            #Jiahong Wu: added error message 
            log.error('Error in cache uploading. Message type %s (%s).' % (type(exc).__name__, exc))
            raise


    def wait(self):
        '''Wait until an object has been transferred
        
        If there are no objects in transit, return immediately. This method
        releases the global lock.
        '''

        if not self.transfer_in_progress():
            return

        with lock_released:
            self.transfer_completed.wait()

    def upload(self, el):
        '''Upload cache entry `el` asynchronously
        
        Return (uncompressed) size of cache entry.
        
        This method releases the global lock.
        '''

        log.debug('upload(%s): start', el)

        #Jiahong (5/7/12): Check if the threads are alive

        assert (el.inode, el.blockno) not in self.in_transit
        self.in_transit.add((el.inode, el.blockno))

        try:
            el.seek(0)
            hash_ = sha256_fh(el)

            try:
                old_block_id = self.db.get_val('SELECT block_id FROM inode_blocks '
                                               'WHERE inode=? AND blockno=?',
                                               (el.inode, el.blockno))
            except NoSuchRowError:
                old_block_id = None

            try:
                block_id = self.db.get_val('SELECT id FROM blocks WHERE hash=?', (hash_,))

            # No block with same hash
            except NoSuchRowError:
                obj_id = self.db.rowid('INSERT INTO objects (refcount, size) VALUES(1, -1)')
                log.debug('upload(%s): created new object %d', el, obj_id)
                block_id = self.db.rowid('INSERT INTO blocks (refcount, obj_id, hash, size) '
                                         'VALUES(?,?,?,?)', (1, obj_id, hash_, el.size))
                log.debug('upload(%s): created new block %d', el, block_id)
                log.debug('upload(%s): adding to upload queue', el)

                # Note: we must finish all db transactions before adding to
                # in_transit, otherwise commit() may return before all blocks
                # are available in db.
                self.db.execute('INSERT OR REPLACE INTO inode_blocks (block_id, inode, blockno) '
                                'VALUES(?,?,?)', (block_id, el.inode, el.blockno))

                self.in_transit.add(obj_id)
                with lock_released:
                    if not self.upload_threads:
                        log.warn("upload(%s): no upload threads, uploading synchronously", el)
                        self._do_upload(el, obj_id)
                    else:
                        self.to_upload.put((el, obj_id))

            # There is a block with the same hash                        
            else:
                if old_block_id == block_id:
                    log.debug('upload(%s): unchanged, block_id=%d', el, block_id)
                    os.fchmod(el.fh.fileno(), stat.S_IRUSR)
                    if el.dirty:
                        self.dirty_size -= el.size
                        self.dirty_entries -= 1
                        if self.dirty_size < 0:
                            self.dirty_size = 0
                        if self.dirty_entries < 0:
                            self.dirty_entries = 0
                    el.dirty = False
                    el.last_upload = time.time()
                    self.in_transit.remove((el.inode, el.blockno))
                    return el.size

                log.debug('upload(%s): (re)linking to %d', el, block_id)
                self.db.execute('UPDATE blocks SET refcount=refcount+1 WHERE id=?',
                                (block_id,))
                self.db.execute('INSERT OR REPLACE INTO inode_blocks (block_id, inode, blockno) '
                                'VALUES(?,?,?)', (block_id, el.inode, el.blockno))
                os.fchmod(el.fh.fileno(), stat.S_IRUSR)
                if el.dirty:
                    self.dirty_size -= el.size
                    self.dirty_entries -= 1
                    if self.dirty_size < 0:
                        self.dirty_size = 0
                    if self.dirty_entries < 0:
                        self.dirty_entries = 0

                el.dirty = False
                el.last_upload = time.time()
                self.in_transit.remove((el.inode, el.blockno))
        except:
            self.in_transit.remove((el.inode, el.blockno))
            raise

        # Check if we have to remove an old block
        if not old_block_id:
            log.debug('upload(%s): no old block, returning', el)
            return el.size

        refcount = self.db.get_val('SELECT refcount FROM blocks WHERE id=?', (old_block_id,))
        if refcount > 1:
            log.debug('upload(%s):  decreased refcount for prev. block: %d', el, old_block_id)
            self.db.execute('UPDATE blocks SET refcount=refcount-1 WHERE id=?', (old_block_id,))
            return el.size

        log.debug('upload(%s): removing prev. block %d', el, old_block_id)
        old_obj_id = self.db.get_val('SELECT obj_id FROM blocks WHERE id=?', (old_block_id,))
        self.db.execute('DELETE FROM blocks WHERE id=?', (old_block_id,))
        refcount = self.db.get_val('SELECT refcount FROM objects WHERE id=?', (old_obj_id,))
        if refcount > 1:
            log.debug('upload(%s):  decreased refcount for prev. obj: %d', el, old_obj_id)
            self.db.execute('UPDATE objects SET refcount=refcount-1 WHERE id=?',
                            (old_obj_id,))
            return el.size

        log.debug('upload(%s): removing object %d', el, old_obj_id)
        self.db.execute('DELETE FROM objects WHERE id=?', (old_obj_id,))

        while old_obj_id in self.in_transit:
            log.debug('upload(%s): waiting for transfer of old object %d to complete',
                      el, old_obj_id)
            self.wait()

        with lock_released:
            if not self.removal_threads:
                log.warn("upload(%s): no removal threads, removing synchronously", el)
                self._do_removal(old_obj_id)
            else:
                log.debug('upload(%s): adding %d to removal queue', el, old_obj_id)
                self.to_remove.put(old_obj_id)

        return el.size


    def transfer_in_progress(self):
        '''Return True if there are any blocks in transit'''

        return len(self.in_transit) > 0

    def _removal_loop(self):
        '''Process removal queue'''

        while True:
            tmp = self.to_remove.get()

            if tmp is QuitSentinel:
                break

            self._do_removal(tmp)

    def _do_removal(self, obj_id):
        '''Remove object'''

#Jiahong: adding exception handling.....
        while True:
            try:
                with self.bucket_pool() as bucket:
                    bucket.delete('s3ql_data_%d' % obj_id)
                break
            except:
                if self.going_down:
                    raise
                log.warning('warning: Block delete timed out. Retrying in 10 seconds.')
                time.sleep(10)
                while True:
                    try:
                        with self.bucket_pool() as bucket:
                            bucket.bucket.conn.close()
                            bucket.bucket.conn = bucket.bucket._get_conn()
                        break
                    except:
                        if self.going_down:
                            raise
                        log.error('Network may be disconnected. Retrying in 10 seconds.')
                        time.sleep(10)



    @contextmanager
    def get(self, inode, blockno):
        """Get file handle for block `blockno` of `inode`
        
        This method releases the global lock, and the managed block
        may do so as well.
        
        Note: if `get` and `remove` are called concurrently, then it is
        possible that a block that has been requested with `get` and
        passed to `remove` for deletion will not be deleted.
        """

        #log.debug('get(inode=%d, block=%d): start', inode, blockno)

        if self.size > self.max_size or len(self.entries) > self.max_entries:
            self.expire()

        el = None
        while el is None:
            # Don't allow changing objects while they're being uploaded
            if (inode, blockno) in self.in_transit:
                log.debug('get(inode=%d, block=%d): inode/blockno in transit, waiting',
                          inode, blockno)
                self.wait()
                continue

            try:
                el = self.entries[(inode, blockno)]

            # Not in cache
            except KeyError:
                filename = os.path.join(self.path, '%d-%d' % (inode, blockno))

                if os.access(filename,os.F_OK): # If cache file already in cache directory, use that
                    with lock_released:
                        el = CacheEntry(inode, blockno, filename, False)
                    self.entries[(inode, blockno)] = el
                    self.size += el.size
                    if el.dirty:
                        self.dirty_size += el.size
                        self.dirty_entries += 1


            try:
                el = self.entries[(inode, blockno)]

            # Not in cache
            except KeyError:
                filename = os.path.join(self.path, '%d-%d' % (inode, blockno))

                try:
                    
                    block_id = self.db.get_val('SELECT block_id FROM inode_blocks '
                                               'WHERE inode=? AND blockno=?', (inode, blockno))

                # No corresponding object
                except NoSuchRowError:
                    #log.debug('get(inode=%d, block=%d): creating new block', inode, blockno)
                    el = CacheEntry(inode, blockno, filename)
                    self.entries[(inode, blockno)] = el

                # Need to download corresponding object
                else:
                    #log.debug('get(inode=%d, block=%d): downloading block', inode, blockno)
                    obj_id = self.db.get_val('SELECT obj_id FROM blocks WHERE id=?', (block_id,))

                    if obj_id in self.in_transit:
                        log.debug('get(inode=%d, block=%d): object %d in transit, waiting',
                                  inode, blockno, obj_id)
                        self.wait()
                        continue

                    # We need to download
                    self.in_transit.add(obj_id)
                    log.debug('get(inode=%d, block=%d): downloading object %d..',
                              inode, blockno, obj_id)
                    def do_read(fh):
                        el = CacheEntry(inode, blockno, filename)
                        #Jiahong: At the start of the download, mark the group read bit to specify that this block is being downloaded
                        el.download_set()
                        shutil.copyfileobj(fh, el, BUFSIZE)
                        return el
                    try:
                        #Jiahong: added retry mechanism to perform_read
#Jiahong (5/4/12): Implemented a mechanism for labeling partially downloaded block objects. Such objects are removed during fsck
                        with lock_released:
                            no_attempts=0
                            while no_attempts < 10:
                                try:
                                    with self.bucket_pool() as bucket:
                                        el = bucket.perform_read(do_read, 's3ql_data_%d' % obj_id)
                                    break
                                except Exception as exc:
                                    if no_attempts >= 9:
                                        log.error('Read cache block error timed out....')
                                        raise(llfuse.FUSEError(errno.EIO))
                                    log.warn('Read s3ql_data_%d error type %s (%s), retrying' % (obj_id, type(exc).__name__, exc))
                                    no_attempts += 1
                                    if el is not None:
                                        el.unlink()
                                    time.sleep(5)
                                    try:
                                        with self.bucket_pool() as bucket:
                                            bucket.bucket.conn.close()
                                            bucket.bucket.conn = bucket.bucket._get_conn()
                                    except:
                                        log.error('Network may be down.')
                                        raise(llfuse.FUSEError(errno.EIO))
                                    

                        # Note: We need to do this *before* releasing the global
                        # lock to notify other threads
                        self.entries[(inode, blockno)] = el

                        # Writing will have set dirty flag
                        os.fchmod(el.fh.fileno(), stat.S_IRUSR)
                        el.dirty = False

                        self.size += el.size
                        if el.dirty:
                            self.dirty_size += el.size
                            self.dirty_entries += 1

                    except NoSuchObject:
                        raise QuietError('Backend claims that object %d does not exist, data '
                                         'may be corrupted or inconsisten. fsck required.'
                                         % obj_id)
                        
                    except:
                        if el is not None:
                            el.unlink()
                        raise
                    finally:
                        self.in_transit.remove(obj_id)
                        with lock_released:
                            self.transfer_completed.notify_all()

            # In Cache
            else:
                #log.debug('get(inode=%d, block=%d): in cache', inode, blockno)
                self.entries.to_head((inode, blockno))

        #el.last_access = time.time()
        oldsize = el.size
        was_dirty = el.dirty

        # Provide fh to caller
        try:
            #log.debug('get(inode=%d, block=%d): yield', inode, blockno)
            yield el
        finally:
            # Update cachesize 
            self.size += el.size - oldsize
            if was_dirty:
                self.dirty_size += el.size - oldsize
            elif el.dirty:
                self.dirty_size += el.size
                self.dirty_entries += 1
            if self.dirty_size < 0:
                self.dirty_size = 0
            if self.dirty_entries < 0:
                self.dirty_entries = 0

#TODO: the parameter value 0.8 may be adjustable by users in the future
            if self.dirty_size > 0.8*self.max_size or self.dirty_entries > 0.8*self.max_entries:
                self.forced_upload = True

        #log.debug('get(inode=%d, block=%d): end', inode, blockno)


#Currently may not being used
#This is the new expire() that only expires clean blocks. In theory, we will always have enough clean block size and entries for a replacement.
#(assuming that the limits on dirty block size and entries are correctly implemented).
    def expire1(self):
        """Perform cache expiry

        This method releases the global lock.
        """

        # Note that we have to make sure that the cache entry is written into
        # the database before we remove it from the cache!

        log.debug('expire: start')

        did_nothing_count = 0
        while (len(self.entries) > self.max_entries or
               (len(self.entries) > 0  and self.size > self.max_size)):

            need_size = self.size - self.max_size
            need_entries = len(self.entries) - self.max_entries

            # Try to expire entries that are not dirty
            for el in self.entries.values_rev():
                if el.dirty:
                    continue

                del self.entries[(el.inode, el.blockno)]
                el.close()
                el.unlink()
                need_entries -= 1
                self.size -= el.size
                need_size -= el.size

                did_nothing_count = 0
                if need_size <= 0 and need_entries <= 0:
                    break

            if need_size <= 0 and need_entries <= 0:
                break

            did_nothing_count += 1
            if did_nothing_count > 50:
                log.error('Problem in expire()')
                break

            # Wait for the next entry
            log.debug('expire: waiting for transfer threads..')
            self.wait() # Releases global lock

        log.debug('expire: end')



#the original expire(), the one that also expires dirty blocks
    def expire(self):
        """Perform cache expiry
        
        This method releases the global lock.
        """

        # Note that we have to make sure that the cache entry is written into
        # the database before we remove it from the cache!

        log.debug('expire: start')
#Jiahong: TODO: put some mechanism here to check for network connection before
#actually trying to expire any blocks

        try:
            with self.bucket_pool() as bucket:
                bucket.store('cloud_gw_test_connection','nodata')
        except:
            log.error('Network appears to be down. Failing expire cache.')
            raise(llfuse.FUSEError(errno.EIO))
#Implement the rest...


        did_nothing_count = 0
        while (len(self.entries) > self.max_entries or
               (len(self.entries) > 0  and self.size > self.max_size)):

            need_size = self.size - self.max_size
            need_entries = len(self.entries) - self.max_entries

            # Try to expire entries that are not dirty
            for el in self.entries.values_rev():
                if el.dirty:
                    continue #Continue to scan clean entries
                    #commented out the following six lines. Do not try to distinguish the two now
                    #if (el.inode, el.blockno) in self.in_transit:
                    #    log.debug('expire: %s is dirty, but already being uploaded', el)
                    #    continue
                    #else:
                    #    log.debug('expire: %s is dirty, trying to flush', el)
                    #    break

                del self.entries[(el.inode, el.blockno)]
                el.close()
                el.unlink()
                need_entries -= 1
                self.size -= el.size
                need_size -= el.size

                did_nothing_count = 0
                if need_size <= 0 and need_entries <= 0:
                    break

            if need_size <= 0 and need_entries <= 0:
                break

            # Try to upload just enough
            for el in self.entries.values_rev():
                if el.dirty and (el.inode, el.blockno) not in self.in_transit:
                    log.debug('expire: uploading %s..', el)
                    freed = self.upload(el) # Releases global lock
                    need_size -= freed
                    did_nothing_count = 0
                else:
                    log.debug('expire: %s can soon be expired..', el)
                    need_size -= el.size
                need_entries -= 1

                if need_size <= 0 and need_entries <= 0:
                    break

            did_nothing_count += 1
            if did_nothing_count > 50:
                log.error('Problem in expire()')
                break

            # Wait for the next entry  
            log.debug('expire: waiting for transfer threads..')
            self.wait() # Releases global lock

        log.debug('expire: end')


    def remove(self, inode, start_no, end_no=None):
        """Remove blocks for `inode`
        
        If `end_no` is not specified, remove just the `start_no` block.
        Otherwise removes all blocks from `start_no` to, but not including,
         `end_no`. 
        
        This method releases the global lock.
        
        Note: if `get` and `remove` are called concurrently, then it is possible
        that a block that has been requested with `get` and passed to `remove`
        for deletion will not be deleted.
        """

        log.debug('remove(inode=%d, start=%d, end=%s): start', inode, start_no, end_no)

        if end_no is None:
            end_no = start_no + 1

        for blockno in range(start_no, end_no):
            # We can't use self.mlock here to prevent simultaneous retrieval
            # of the block with get(), because this could deadlock
            if (inode, blockno) in self.entries:
                log.debug('remove(inode=%d, blockno=%d): removing from cache',
                          inode, blockno)

                # Type inference fails here
                #pylint: disable-msg=E1103
                el = self.entries.pop((inode, blockno))

                self.size -= el.size
                if el.dirty:
                    self.dirty_size -= el.size
                    self.dirty_entries -= 1
                    if self.dirty_size < 0:
                        self.dirty_size = 0
                    if self.dirty_entries < 0:
                        self.dirty_entries = 0


                el.unlink()

            try:
                block_id = self.db.get_val('SELECT block_id FROM inode_blocks '
                                           'WHERE inode=? AND blockno=?', (inode, blockno))
            except NoSuchRowError:
                log.debug('remove(inode=%d, blockno=%d): block not in db', inode, blockno)
                continue

            # Detach inode from block
            self.db.execute('DELETE FROM inode_blocks WHERE inode=? AND blockno=?',
                            (inode, blockno))

            # Decrease block refcount
            refcount = self.db.get_val('SELECT refcount FROM blocks WHERE id=?', (block_id,))
            if refcount > 1:
                log.debug('remove(inode=%d, blockno=%d): decreasing refcount for block %d',
                          inode, blockno, block_id)
                self.db.execute('UPDATE blocks SET refcount=refcount-1 WHERE id=?',
                                (block_id,))
                continue

            # Detach block from object
            log.debug('remove(inode=%d, blockno=%d): deleting block %d',
                      inode, blockno, block_id)
            obj_id = self.db.get_val('SELECT obj_id FROM blocks WHERE id=?', (block_id,))
            self.db.execute('DELETE FROM blocks WHERE id=?', (block_id,))

            # Decrease object refcount
            refcount = self.db.get_val('SELECT refcount FROM objects WHERE id=?', (obj_id,))
            if refcount > 1:
                log.debug('remove(inode=%d, blockno=%d): decreasing refcount for object %d',
                          inode, blockno, obj_id)
                self.db.execute('UPDATE objects SET refcount=refcount-1 WHERE id=?',
                                (obj_id,))
            else:
                while obj_id in self.in_transit:
                    log.debug('remove(inode=%d, blockno=%d): waiting for transfer of '
                              'object %d to complete', inode, blockno, obj_id)
                    self.wait()
                log.debug('remove(inode=%d, blockno=%d): deleting object %d',
                          inode, blockno, obj_id)
                self.db.execute('DELETE FROM objects WHERE id=?', (obj_id,))
                with lock_released:
                    if not self.removal_threads:
                        log.warn("remove(): no removal threads, removing synchronously")
                        self._do_removal(obj_id)
                    else:
                        self.to_remove.put(obj_id)

        log.debug('remove(inode=%d, start=%d, end=%s): end', inode, start_no, end_no)

    def flush(self, inode):
        """Flush buffers for `inode`"""

        # Cache entries are automatically flushed after each read() and write()
        pass



#TODO: May rename/remove this function if we do not require dirty blocks to be uploaded before a snapshot is taken
    def commit(self):
        """Initiate upload of all dirty blocks
        
        When the method returns, all blocks have been registered
        in the database (but the actual uploads may still be 
        in progress).
        
        This method releases the global lock.
        """

        for el in self.entries.itervalues():
            if not (el.dirty and (el.inode, el.blockno) not in self.in_transit):
                continue

            self.upload(el) # Releases global lock

#Jia-Hong (4/18/12): Keep the reference to this function from ctrl.py (may need immediate cache flush at some point).
    def clear(self):
        """Clear cache
        
        This method releases the global lock.
        """

        log.debug('clear: start')
        bak = self.max_entries
        self.max_entries = 0
#this uses the original expire(), the one that expires dirty blocks too
        self.expire() # Releases global lock
        self.max_entries = bak

        log.debug('clear: end')

#New function to close all opened cache files
    def close_cache(self):

        for el in self.entries.values_rev():
            el.close()

    def __del__(self):
        if len(self.entries) > 0:
            raise RuntimeError("BlockCache instance was destroyed without calling destroy()!")
