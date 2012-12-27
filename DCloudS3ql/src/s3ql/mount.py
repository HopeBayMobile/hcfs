'''
mount.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) 2008-2009 Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
Modified by Jia-Hong Wu to allow changing the number of cache entries under sudo mode (4/16/2012)
Jia-Hong marking the code for future edits on 4/16/2012
TODO: min_obj_size does not appear to have any real impact. There could be many small cache files,
TODO: hence it will be necessary to close opened cache files if we want the number of cache entries to
TODO: exceed the maximum number of opened files. How and when to close opened files and reopen them
TODO: will need to be investigated. Meanwhile, we might settle for limiting both the size and entries
TODO: for dirty cache, and disallow writing if any of the two is exceeded.
'''

from __future__ import division, print_function, absolute_import
from . import fs, CURRENT_FS_REV
from .backends.common import get_bucket_factory, BucketPool, NoSuchBucket
from .block_cache import BlockCache
from .common import (setup_logging, get_bucket_cachedir, get_seq_no, QuietError, stream_write_bz2, 
    stream_read_bz2, GlobalVarContainer)
from .daemonize import daemonize
from .database import Connection
from .inode_cache import InodeCache
from .metadata import cycle_metadata, dump_metadata, restore_metadata
from .parse_args import ArgumentParser
from threading import Thread
import cPickle as pickle
import llfuse
import logging
import os
import signal
import stat
import sys
import tempfile
import thread
import threading
import time
import resource
    

log = logging.getLogger("mount")

def install_thread_excepthook():
    """work around sys.excepthook thread bug
    
    See http://bugs.python.org/issue1230540.

    Call once from __main__ before creating any threads. If using
    psyco, call psyco.cannotcompile(threading.Thread.run) since this
    replaces a new-style class method.
    """

    init_old = threading.Thread.__init__
    def init(self, *args, **kwargs):
        init_old(self, *args, **kwargs)
        run_old = self.run
        def run_with_except_hook(*args, **kw):
            try:
                run_old(*args, **kw)
            except SystemExit:
                raise
            except:
                sys.excepthook(*sys.exc_info())
        self.run = run_with_except_hook

    threading.Thread.__init__ = init
install_thread_excepthook()

def main(args=None):
    '''Mount S3QL file system'''

    if args is None:
        args = sys.argv[1:]

    options = parse_args(args)

#Jiahong on 9/30/12: Mod this to make the cache entry number huge
#Jiahong on 11/5/12: Do not check the max_cache_entries. Should do this by product.
    #if options.max_cache_entries > 9990000:
    #    raise QuietError('Too many cache entries (max 9990000)')

    resource.setrlimit(resource.RLIMIT_NOFILE,(300000,400000))

    # Save handler so that we can remove it when daemonizing

    stdout_log_handler = setup_logging(options)

    if options.threads is None:
        options.threads = determine_threads(options)

    if not os.path.exists(options.mountpoint):
        raise QuietError('Mountpoint does not exist.')

    if options.profile:
        import cProfile
        import pstats
        prof = cProfile.Profile()

    bucket_factory = get_bucket_factory(options)
    bucket_pool = BucketPool(bucket_factory)

    # Get paths
    cachepath = get_bucket_cachedir(options.storage_url, options.cachedir)

    # Retrieve metadata
    try:
        with bucket_pool() as bucket:
            (param, db) = get_metadata(bucket, cachepath)
    except NoSuchBucket as exc:
        raise QuietError(str(exc))

    if param['max_obj_size'] < options.min_obj_size:
        raise QuietError('Maximum object size must be bigger than minimum object size.')

    if options.nfs:
        # NFS may try to look up '..', so we have to speed up this kind of query
        log.info('Creating NFS indices...')
        db.execute('CREATE INDEX IF NOT EXISTS ix_contents_inode ON contents(inode)')

    else:
        db.execute('DROP INDEX IF EXISTS ix_contents_inode')

    # create a global var container. all global vars can be store in it
    var_container = GlobalVarContainer()
    
    metadata_upload_thread = MetadataUploadThread(bucket_pool, param, db,
                                                  options.metadata_upload_interval, var_container)
    block_cache = BlockCache(bucket_pool, db, cachepath + '-cache',
                             options.cachesize * 1024, options.max_cache_entries)
    commit_thread = CommitThread(block_cache)
    closecache_thread = CloseCacheThread(block_cache)
    operations = fs.Operations(block_cache, db, var_container, max_obj_size=param['max_obj_size'],
                               inode_cache=InodeCache(db, param['inode_gen']),
                               upload_event=metadata_upload_thread.event)

    log.info('Mounting filesystem...')
    llfuse.init(operations, options.mountpoint, get_fuse_opts(options))


    if not options.fg:
        if stdout_log_handler:
            logging.getLogger().removeHandler(stdout_log_handler)
        daemonize(options.cachedir)

    exc_info = setup_exchook()

    # After we start threads, we must be sure to terminate them
    # or the process will hang 
    try:
        block_cache.init(options.threads)
        metadata_upload_thread.start()
        commit_thread.start()
        closecache_thread.start()

        if options.upstart:
            os.kill(os.getpid(), signal.SIGSTOP)
        if options.profile:
            prof.runcall(llfuse.main, options.single)
        else:
            llfuse.main(options.single)

        # Re-raise if main loop terminated due to exception in other thread
        # or during cleanup, but make sure we still unmount file system
        # (so that Operations' destroy handler gets called)
        if exc_info:
            (tmp0, tmp1, tmp2) = exc_info
            exc_info[:] = []
            raise tmp0, tmp1, tmp2

        log.info("FUSE main loop terminated.")

    except:
        # Tell finally block not to raise any additional exceptions
        exc_info[:] = sys.exc_info()

        log.warn('Encountered exception, trying to clean up...')

        # We do *not* free the mountpoint on exception. Why? E.g. if someone is
        # mirroring the mountpoint, and it suddenly becomes empty, all the
        # mirrored data will be deleted. However, it's crucial to still call
        # llfuse.close, so that Operations.destroy() can flush the inode cache.
        try:
            log.info("Unmounting file system...")
            with llfuse.lock:
                llfuse.close(unmount=False)
        except:
            log.exception("Exception during cleanup:")

        raise

    else:
        # llfuse.close() still needs block_cache.
        log.info("Unmounting file system...")
        with llfuse.lock:
            llfuse.close()

    # Terminate threads
    finally:
        # wthung, 2012/11/28
        # ensure dirty cache and to_remove is empty before terminating work threads
        # this is a block function
        # Jiahong (12/21/12): Will need to review this cleanup operation
        #log.debug("Waiting for dirty cache to be cleaned and remove queue to become empty...")
        #do_cleanup(block_cache)
        
        log.debug("Waiting for background threads...")
        for (op, with_lock) in ((metadata_upload_thread.stop, False),
                                (commit_thread.stop, False),
                                (closecache_thread.stop, False),
                                (block_cache.destroy, True),
                                (metadata_upload_thread.join, False),
                                (commit_thread.join, False),
                                (closecache_thread.join, False)):
            try:
                if with_lock:
                    with llfuse.lock:
                        op()
                else:
                    op()
            except:
                # We just live with the race cond here
                if not exc_info:
                    exc_info = sys.exc_info()
                else:
                    log.exception("Exception during cleanup:")

        log.debug("All background threads terminated.")

    # Re-raise if there's been an exception during cleanup
    # (either in main thread or other thread)
    if exc_info:
        raise exc_info[0], exc_info[1], exc_info[2]

    # At this point, there should be no other threads left

    # Do not update .params yet, dump_metadata() may fail if the database is
    # corrupted, in which case we want to force an fsck.
    param['max_inode'] = db.get_val('SELECT MAX(id) FROM inodes')
    with bucket_pool() as bucket:
        seq_no = get_seq_no(bucket)
        if metadata_upload_thread.db_mtime == os.stat(cachepath + '.db').st_mtime:
            log.info('File system unchanged, not uploading metadata.')
            del bucket['s3ql_seq_no_%d' % param['seq_no']]
            param['seq_no'] -= 1
            pickle.dump(param, open(cachepath + '.params', 'wb'), 2)
        elif seq_no == param['seq_no']:
            cycle_metadata(bucket)
            param['last-modified'] = time.time()

            log.info('Dumping metadata...')
            fh = tempfile.TemporaryFile()
            dump_metadata(db, fh)
            def do_write(obj_fh):
                fh.seek(0)
                stream_write_bz2(fh, obj_fh)
                return obj_fh

            log.info("Compressing and uploading metadata...")
            obj_fh = bucket.perform_write(do_write, "s3ql_metadata", metadata=param,
                                          is_compressed=True)
            log.info('Wrote %.2f MB of compressed metadata.', obj_fh.get_obj_size() / 1024 ** 2)
            pickle.dump(param, open(cachepath + '.params', 'wb'), 2)
            self.var_container.dirty_metadata = False
        else:
            log.error('Remote metadata is newer than local (%d vs %d), '
                      'refusing to overwrite!', seq_no, param['seq_no'])
            log.error('The locally cached metadata will be *lost* the next time the file system '
                      'is mounted or checked and has therefore been backed up.')
            for name in (cachepath + '.params', cachepath + '.db'):
                for i in reversed(range(4)):
                    if os.path.exists(name + '.%d' % i):
                        os.rename(name + '.%d' % i, name + '.%d' % (i + 1))
                os.rename(name, name + '.0')

    db.execute('ANALYZE')
    db.execute('VACUUM')
    db.close()

    if options.profile:
        tmp = tempfile.NamedTemporaryFile()
        prof.dump_stats(tmp.name)
        fh = open('s3ql_profile.txt', 'w')
        p = pstats.Stats(tmp.name, stream=fh)
        tmp.close()
        p.strip_dirs()
        p.sort_stats('cumulative')
        p.print_stats(50)
        p.sort_stats('time')
        p.print_stats(50)
        fh.close()

def do_cleanup(block_cache):
    '''
    Ensure dirty cache is completely flushed to cloud storage and
    remove queue is empty. A statistic file will be dumped.
    '''
    data_file = '/dev/shm/cleanup_data'
    if os.path.exists(data_file):
        os.system('sudo rm -f %s' % data_file)
    
    # if backend is disconnected, quit
    if not block_cache.network_ok:
        log.debug("Network is down before doing cleanup jobs.")
        return
    
    orig_dirty_size = block_cache.dirty_size
    orig_remove_qsize = block_cache.to_remove.qsize()
    cur_dirty_size = orig_dirty_size
    cur_remove_qsize = orig_remove_qsize
    prev_completeness = -1
    stops = 0
    
    if cur_dirty_size == 0:
        completeness1 = 100
    else:
        completeness1 = 0
    if cur_remove_qsize == 0:
        completeness2 = 100
    else:
        completeness2 = 0
    
    while cur_dirty_size > 0 or cur_remove_qsize > 0:
        # ensure s3ql is able to upload
        block_cache.do_upload = True
        
        time.sleep(1)
        cur_dirty_size = block_cache.dirty_size
        cur_remove_qsize = block_cache.to_remove.qsize()
        
        if cur_dirty_size > 0:
            completeness1 = ((orig_dirty_size - cur_dirty_size) / orig_dirty_size) * 100
        if cur_remove_qsize > 0:
            completeness2 = ((orig_remove_qsize - cur_remove_qsize) / orig_remove_qsize) * 100
        
        completeness = min(completeness1, completeness2)
        if prev_completeness != completeness:
            prev_completeness = completeness
            stops = 0
        else:
            # value of completeness isn't changed. increase counter
            stops = stops + 1
        
        # echo statistics to a file
        os.system("echo '%d' > %s" % (completeness, data_file))
        
        # if value of completeness isn't changed for 1 min
        if stops > 60:
            log.debug("Cleanup progress isn't changed for 1 min.")
            # check what's happened
            # backend is connected?
            if block_cache.network_ok:
                log.debug("Network is OK. Wait for another 1 min.")
                # wait for more 5 mins. if problem remains, quit
                if stops > 120:
                    log.debug("Cleanup progress isn't changed for a long while. Quit.")
                    break
            else:
                # wait for 30 secs to check network again
                time.sleep(30)
                if not block_cache.network_ok:
                    # network is still not ok, quit
                    log.debug("Network is down when doing cleanup jobs. Quit.")
                    break
            
    # 100% completed when exiting while loop
    os.system("echo '100' > %s" % data_file)
        
def determine_threads(options):
    '''Return optimum number of upload threads'''

    cores = os.sysconf('SC_NPROCESSORS_ONLN')
    memory = os.sysconf('SC_PHYS_PAGES') * os.sysconf('SC_PAGESIZE')

    if options.compress == 'lzma':
        # Keep this in sync with compression level in backends/common.py
        # Memory usage according to man xz(1)
        mem_per_thread = 186 * 1024 ** 2
    else:
        # Only check LZMA memory usage
        mem_per_thread = 0

    if cores == -1 or memory == -1:
        log.warn("Can't determine number of cores, using 2 upload threads.")
        return 1
    elif 2 * cores * mem_per_thread > (memory / 2):
        threads = min(int((memory / 2) // mem_per_thread), 10)
        if threads > 0:
            log.info('Using %d upload threads (memory limited).', threads)
        else:
            log.warn('Warning: compression will require %d MB memory '
                     '(%d%% of total system memory', mem_per_thread / 1024 ** 2,
                     mem_per_thread * 100 / memory)
            threads = 1
        return threads
    else:
        threads = min(2 * cores, 10)
        log.info("Using %d upload threads.", threads)
        return threads

def get_metadata(bucket, cachepath):
    '''Retrieve metadata'''

    seq_no = get_seq_no(bucket)

    # Check for cached metadata
    db = None
    if os.path.exists(cachepath + '.params'):
        param = pickle.load(open(cachepath + '.params', 'rb'))
        if param['seq_no'] < seq_no:
            log.info('Ignoring locally cached metadata (outdated).')
            param = bucket.lookup('s3ql_metadata')
        else:
            log.info('Using cached metadata.')
            db = Connection(cachepath + '.db')
    else:
        param = bucket.lookup('s3ql_metadata')

    # Check for unclean shutdown
    if param['seq_no'] < seq_no:
        raise QuietError('Backend reports that fs is still mounted elsewhere, aborting.')       

    # Check revision
    if param['revision'] < CURRENT_FS_REV:
        raise QuietError('File system revision too old, please run `s3qladm upgrade` first.')
    elif param['revision'] > CURRENT_FS_REV:
        raise QuietError('File system revision too new, please update your '
                         'S3QL installation.')

    # Check that the fs itself is clean
    #Jiahong: Adding customized messages....
    if param['needs_fsck']:
        log.error("[error] File system damaged or not unmounted cleanly, reboot gateway to initiate fsck!")
        raise QuietError("File system damaged or not unmounted cleanly, reboot gateway to initiate fsck!")
    if time.time() - param['last_fsck'] > 60 * 60 * 24 * 31:
        log.warn('Last file system check was more than 1 month ago, '
                 'running fsck.s3ql is recommended.')

    if  param['max_inode'] > 2 ** 32 - 50000:
        raise QuietError('Insufficient free inodes, fsck run required.')
    elif param['max_inode'] > 2 ** 31:
        log.warn('Few free inodes remaining, running fsck is recommended')

    # Download metadata
    if not db:
        def do_read(fh):
            tmpfh = tempfile.TemporaryFile()
            stream_read_bz2(fh, tmpfh)
            return tmpfh
        log.info('Downloading and decompressing metadata...')
        tmpfh = bucket.perform_read(do_read, "s3ql_metadata")
        os.close(os.open(cachepath + '.db.tmp', os.O_RDWR | os.O_CREAT | os.O_TRUNC,
                         stat.S_IRUSR | stat.S_IWUSR))
        db = Connection(cachepath + '.db.tmp', fast_mode=False)
        log.info("Reading metadata...")
        tmpfh.seek(0)
        restore_metadata(tmpfh, db)
        db.close()
        os.rename(cachepath + '.db.tmp', cachepath + '.db')
        db = Connection(cachepath + '.db')

    # Increase metadata sequence no 
    param['seq_no'] += 1
    param['needs_fsck'] = True
    bucket['s3ql_seq_no_%d' % param['seq_no']] = 'Empty'
    pickle.dump(param, open(cachepath + '.params', 'wb'), 2)
    param['needs_fsck'] = False

    return (param, db)

def get_fuse_opts(options):
    '''Return fuse options for given command line options'''

    #Jiahong: New config added to fuse mount to allow bigger block r/w
    fuse_opts = [ b"nonempty", b'fsname=%s' % options.storage_url, b'big_writes', 'max_read=131072', 'max_write=131072',
                  'subtype=s3ql' ]

    if options.allow_other:
        fuse_opts.append(b'allow_other')
    if options.allow_root:
        fuse_opts.append(b'allow_root')
    if options.allow_other or options.allow_root:
        fuse_opts.append(b'default_permissions')

    return fuse_opts


def parse_args(args):
    '''Parse command line'''

    # Parse fstab-style -o options
    if '--' in args:
        max_idx = args.index('--')
    else:
        max_idx = len(args)
    if '-o' in args[:max_idx]:
        pos = args.index('-o')
        val = args[pos + 1]
        del args[pos]
        del args[pos]
        for opt in reversed(val.split(',')):
            if '=' in opt:
                (key, val) = opt.split('=')
                args.insert(pos, val)
                args.insert(pos, '--' + key)
            else:
                if opt in ('rw', 'defaults', 'auto', 'noauto', 'user', 'nouser', 'dev', 'nodev',
                           'suid', 'nosuid', 'atime', 'diratime', 'exec', 'noexec', 'group',
                           'mand', 'nomand', '_netdev', 'nofail', 'norelatime', 'strictatime',
                           'owner', 'users', 'nobootwait'):
                    continue
                elif opt == 'ro':
                    raise QuietError('Read-only mounting not supported.')
                args.insert(pos, '--' + opt)

    parser = ArgumentParser(
        description="Mount an S3QL file system.")

    parser.add_log('~/.s3ql/mount.log')
    parser.add_cachedir()
    parser.add_authfile()
    parser.add_debug_modules()
    parser.add_quiet()
    parser.add_ssl()
    parser.add_version()
    parser.add_storage_url()

    parser.add_argument("mountpoint", metavar='<mountpoint>', type=os.path.abspath,
                        help='Where to mount the file system')
    parser.add_argument("--cachesize", type=int, default=102400, metavar='<size>',
                      help="Cache size in kb (default: 102400 (100 MB)). Should be at least 10 times "
                      "the maximum object size of the filesystem, otherwise an object may be retrieved "
                      "and written several times during a single write() or read() operation.")
    parser.add_argument("--max-cache-entries", type=int, default=768, metavar='<num>',
                      help="Maximum number of entries in cache (default: %(default)d). "
                      'Each cache entry requires one file descriptor, so if you increase '
                      'this number you have to make sure that your process file descriptor '
                      'limit (as set with `ulimit -n`) is high enough (at least the number '
                      'of cache entries + 100).')
    parser.add_argument("--min-obj-size", type=int, default=512, metavar='<size>',
                      help="Minimum size of storage objects in KB. Files smaller than this "
                           "may be combined into groups that are stored as single objects "
                           "in the storage backend. Default: %(default)d KB.")
    parser.add_argument("--allow-other", action="store_true", default=False, help=
                      'Normally, only the user who called `mount.s3ql` can access the mount '
                      'point. This user then also has full access to it, independent of '
                      'individual file permissions. If the `--allow-other` option is '
                      'specified, other users can access the mount point as well and '
                      'individual file permissions are taken into account for all users.')
    parser.add_argument("--allow-root", action="store_true", default=False,
                      help='Like `--allow-other`, but restrict access to the mounting '
                           'user and the root user.')
    parser.add_argument("--fg", action="store_true", default=False,
                      help="Do not daemonize, stay in foreground")
    parser.add_argument("--single", action="store_true", default=False,
                      help="Run in single threaded mode. If you don't understand this, "
                           "then you don't need it.")
    parser.add_argument("--upstart", action="store_true", default=False,
                      help="Stay in foreground and raise SIGSTOP once mountpoint "
                           "is up.")
    parser.add_argument("--profile", action="store_true", default=False,
                      help="Create profiling information. If you don't understand this, "
                           "then you don't need it.")
    parser.add_argument("--compress", action="store", default='lzma', metavar='<name>',
                      choices=('lzma', 'bzip2', 'zlib', 'none'),
                      help="Compression algorithm to use when storing new data. Allowed "
                           "values: `lzma`, `bzip2`, `zlib`, none. (default: `%(default)s`)")
    parser.add_argument("--metadata-upload-interval", action="store", type=int,
                      default=24 * 60 * 60, metavar='<seconds>',
                      help='Interval in seconds between complete metadata uploads. '
                           'Set to 0 to disable. Default: 24h.')
    parser.add_argument("--threads", action="store", type=int,
                      default=None, metavar='<no>',
                      help='Number of parallel upload threads to use (default: auto).')
    parser.add_argument("--nfs", action="store_true", default=False,
                      help='Enable some optimizations for exporting the file system '
                           'over NFS. (default: %(default)s)')

    options = parser.parse_args(args)

    if options.allow_other and options.allow_root:
        parser.error("--allow-other and --allow-root are mutually exclusive.")

    if not options.log and not options.fg:
        parser.error("Please activate logging to a file or syslog, or use the --fg option.")

    if options.profile:
        options.single = True

    if options.upstart:
        options.fg = True

    if options.metadata_upload_interval == 0:
        options.metadata_upload_interval = None

    if options.compress == 'none':
        options.compress = None

    return options

class MetadataUploadThread(Thread):
    '''
    Periodically upload metadata. Upload is done every `interval`
    seconds, and whenever `event` is set. To terminate thread,
    set `quit` attribute as well as `event` event.
    
    This class uses the llfuse global lock. When calling objects
    passed in the constructor, the global lock is acquired first.    
    '''

    def __init__(self, bucket_pool, param, db, interval, var_container):
        super(MetadataUploadThread, self).__init__()
        self.bucket_pool = bucket_pool
        self.param = param
        self.db = db
        self.interval = interval
        self.daemon = True
        self.db_mtime = os.stat(db.file).st_mtime
        self.event = threading.Event()
        self.quit = False
        self.name = 'Metadata-Upload-Thread'
        self.var_container = var_container

    def run(self):
        log.debug('MetadataUploadThread: start')

        while not self.quit:
            self.event.wait(self.interval)
            self.event.clear()

            if self.quit:
                break

            with llfuse.lock:
                if self.quit:
                    break
                new_mtime = os.stat(self.db.file).st_mtime
                if self.db_mtime == new_mtime:
                    log.info('File system unchanged, not uploading metadata.')
                    continue

                log.info('Dumping metadata...')
                fh = tempfile.TemporaryFile()
                dump_metadata(self.db, fh)

            #New mod by Jiahong on 5/7/12 and mod again on 10/24/12: check if can successfully get the seq no from the backend before proceeding
            try:
                with self.bucket_pool() as bucket:
                    seq_no = get_seq_no(bucket)
            except:
                log.error('Cannot connect to backend. Skipping metadata upload for now.')
                fh.close()
                continue
            
            #  Jiahong (10/24/12): To handle disconnection during meta backup
            try:
                with self.bucket_pool() as bucket:
                    if seq_no != self.param['seq_no']:
                        log.error('Remote metadata is newer than local (%d vs %d), '
                                  'refusing to overwrite!', seq_no, self.param['seq_no'])
                        fh.close()
                        continue

                    cycle_metadata(bucket)
                    fh.seek(0)
                    self.param['last-modified'] = time.time()

                    # Temporarily decrease sequence no, this is not the final upload
                    self.param['seq_no'] -= 1
                    def do_write(obj_fh):
                        fh.seek(0)
                        stream_write_bz2(fh, obj_fh)
                        return obj_fh
                    log.info("Compressing and uploading metadata...")
                    obj_fh = bucket.perform_write(do_write, "s3ql_metadata", metadata=self.param,
                                                  is_compressed=True)
                    log.info('Wrote %.2f MB of compressed metadata.', obj_fh.get_obj_size() / 1024 ** 2)
                    self.param['seq_no'] += 1

                    fh.close()
                    self.db_mtime = new_mtime
                    self.var_container.dirty_metadata = False
            except:
                log.error('Cannot connect to backend. Skipping metadata upload for now.')
                fh.close()
                continue

        log.debug('MetadataUploadThread: end')

    def stop(self):
        '''Signal thread to terminate'''

        self.quit = True
        self.event.set()

def setup_exchook():
    '''Send SIGTERM if any other thread terminates with an exception
    
    The exc_info will be saved in the list object returned
    by this function.
    '''

    this_thread = thread.get_ident()
    old_exchook = sys.excepthook
    exc_info = []

    def exchook(type_, val, tb):
        if (thread.get_ident() != this_thread
            and not exc_info):
            os.kill(os.getpid(), signal.SIGTERM)
            exc_info.append(type_)
            exc_info.append(val)
            exc_info.append(tb)

            old_exchook(type_, val, tb)

        # If the main thread re-raised exception, there is no need to call
        # excepthook again
        elif not (thread.get_ident() == this_thread
                  and exc_info == [type_, val, tb]):
            old_exchook(type_, val, tb)

    sys.excepthook = exchook

    return exc_info

# Jiahong (11/27/12): TODO, put a close_cache thread class here.
# Use CommitThread as a reference.
class CloseCacheThread(Thread):
    '''
    Closing cache files.

    This class uses two algorithms to close cache files: (1) If
    files are closed, corresponding cache files are also closed.
    (2) If a cache entry is not accessed for 360 seconds, the 
    cache entry is closed.
    '''

    def __init__(self, block_cache):
        super(CloseCacheThread, self).__init__()
        self.block_cache = block_cache
        self.stop_event = threading.Event()
        self.name = 'CloseCacheThread'

    def run(self):
        log.debug('CloseCacheThread: start')

        while not self.stop_event.is_set():
            do_nothing = True
            timestamp = time.time()
            local_cache_to_close = self.block_cache.cache_to_close.copy()
            self.block_cache.cache_to_close.clear()
            for (inode_id,block_id) in self.block_cache.opened_entries:
                if self.stop_event.is_set():
                    break

                # Jiahong (12/12/12): If need to keep the block open for now, skip the check
                if (inode_id, block_id) in self.block_cache.in_transit:
                    continue

                #  First check if this block is scheduled for closing due to file closing
                if len(local_cache_to_close) > 0:
                    if inode_id in local_cache_to_close:
                        with llfuse.lock:
                            try:
                                if self.block_cache.entries[(inode_id,block_id)].isopen is True:
                                    self.block_cache.entries[(inode_id,block_id)].close()
                                    del self.block_cache.opened_entries[(inode_id,block_id)]
                                log.debug('Closed block (%d, %d) due to file close' % (inode_id,block_id))
                                do_nothing = False
                            except:
                                log.debug('Skipping block (%d, %d), already gone?' % (inode_id,block_id))
                                pass
                        continue

                #  Then check if there are cache files sitting idle for over 360 seconds
                if timestamp - self.block_cache.entries[(inode_id,block_id)].last_access > 360:
                    with llfuse.lock:
                        try:
                            if self.block_cache.entries[(inode_id,block_id)].isopen is True:
                                self.block_cache.entries[(inode_id,block_id)].close()
                                del self.block_cache.opened_entries[(inode_id,block_id)]
                            log.debug('Closed block (%d, %d) due to idle time' % (inode_id,block_id))
                            do_nothing = False
                        except:
                            log.debug('Skipping block (%d, %d), already gone?' % (inode_id,block_id))
                            pass
            local_cache_to_close.clear()

            if do_nothing:
                self.stop_event.wait(60)

        log.debug('CloseCacheThread: end')

    def stop(self):
        '''Signal thread to terminate'''

        self.stop_event.set()


class CommitThread(Thread):
    '''
    Periodically upload dirty blocks.
    
    This class uses the llfuse global lock. When calling objects
    passed in the constructor, the global lock is acquired first.
    '''


    def __init__(self, block_cache):
        super(CommitThread, self).__init__()
        self.block_cache = block_cache
        self.stop_event = threading.Event()
        self.name = 'CommitThread'

# Start/stop of dirty cache uploading is controlled by ctrl.py using uploadon / uploadoff parameters
    def run(self):
        log.debug('CommitThread: start')
        
        with llfuse.lock:
            self.block_cache.read_cachefiles()
        while not self.stop_event.is_set():
            did_sth = False
            #Only upload dirty blocks if scheduled or if dirty cache nearly occupied all allocated cache size
            if self.block_cache.do_upload or self.block_cache.forced_upload or self.block_cache.snapshot_upload:
                stamp = time.time()
                test_connection = 100

                # Jiahong (12/7/12): Adding code to monitor and fix inconsistent number of dirty cache entries reported

                most_recent_access = 0
                have_dirty_cache = False
                total_cache_size = 0
                dirty_cache_size = 0

                for el in self.block_cache.entries.values_rev():
                    if (most_recent_access < el.last_access):
                        most_recent_access = el.last_access
                    if el.dirty:
                        have_dirty_cache = True
                        dirty_cache_size += el.size
                    total_cache_size += el.size

                    if not (self.block_cache.do_upload or self.block_cache.forced_upload or self.block_cache.snapshot_upload):
                        continue
                    if not (el.dirty and (el.inode, el.blockno) not in self.block_cache.in_transit and not el.to_delete):
                        continue
                    #Modified by Jiahong Wu
                    # Wait for one minute since last uploading the block to do it again
                    # TODO: consider new policy on when to upload the block. May need to delay doing
                    # TODO: so if the block or the file is being accessed (either read or write)
                    # Jiahong (10/25/12): Change back to last_access, but now wait for 60 seconds
                    if stamp - el.last_access < 60:
                        continue

                    # Jiahong: (5/7/12) delay upload process if network is down
                    if test_connection >= 100:
                        try:
                            with self.block_cache.bucket_pool() as bucket:
                                bucket.store('cloud_gw_test_connection','nodata')
                            test_connection = 0
                            self.block_cache.network_ok = True
                        except:
                            log.error('Network appears to be down. Delaying cache upload.')
                            self.stop_event.wait(60)
                            self.block_cache.network_ok = False
                            break
                    else:
                        test_connection = test_connection + 1

                    # Acquire global lock to access UploadManager instance
                    with llfuse.lock:
                        if self.stop_event.is_set():
                            break
                        # Object may have been accessed while waiting for lock
                        if not (el.dirty and (el.inode, el.blockno) not in self.block_cache.in_transit):
                            continue
                        self.block_cache.upload(el)
                    did_sth = True

                    if self.stop_event.is_set():
                        break
            else:  # Added by Jiahong on 12/7/12 to handle inconsisteny number of dirty cache entries 
                most_recent_access = 0
                have_dirty_cache = False
                total_cache_size = 0
                dirty_cache_size = 0

                for el in self.block_cache.entries.values_rev():
                    if (most_recent_access < el.last_access):
                        most_recent_access = el.last_access
                    if el.dirty:
                        have_dirty_cache = True
                        dirty_cache_size += el.size

                    total_cache_size += el.size

            if have_dirty_cache:
                log.debug('Computed cache status in committhread: Most_recent_access %d, Have dirty cache, sum cache %d' % (most_recent_access,total_cache_size))
            else:
                log.debug('Computed cache status in committhread: Most_recent_access %d, No dirty cache, sum cache %d' % (most_recent_access,total_cache_size))

            if time.time() - most_recent_access > 60 and ((have_dirty_cache is False and self.block_cache.dirty_entries > 0) or total_cache_size != self.block_cache.size or dirty_cache_size != self.block_cache.dirty_size):
                log.info('Potential cache size inconsistency detected. Conducting sweeping.')
                with llfuse.lock:
                    real_total_cache_size = 0
                    real_dirty_cache_size = 0
                    real_dirty_entries = 0
                    for el in self.block_cache.entries.values_rev():
                        real_total_cache_size += el.size
                        if el.dirty:
                            real_dirty_cache_size += el.size
                            real_dirty_entries += 1
                    log.debug('Computed value after sweeping: %d, %d, %d' % (real_total_cache_size, real_dirty_cache_size, real_dirty_entries))
                    if real_total_cache_size != self.block_cache.size or real_dirty_cache_size != self.block_cache.dirty_size or real_dirty_entries != self.block_cache.dirty_entries:
                        log.error('Inconsistency in cache size detected, correcting to actual value.')
                        self.block_cache.size = real_total_cache_size
                        self.block_cache.dirty_size = real_dirty_cache_size
                        self.block_cache.dirty_entries = real_dirty_entries

            if not did_sth:
                self.stop_event.wait(5)
      
            #Added by Jiahong Wu (5/7/12): Monitor upload threads for alive threads
            if time.time() - self.block_cache.last_checked > 300:
                self.block_cache.check_alive_threads()

        log.debug('CommitThread: end')

    def stop(self):
        '''Signal thread to terminate'''

        self.block_cache.going_down = True
        self.stop_event.set()


if __name__ == '__main__':
    main(sys.argv[1:])
