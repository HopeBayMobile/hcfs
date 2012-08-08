'''
swift.py - this file is part of S3QL (http://s3ql.googlecode.com)

Copyright (C) Nikolaus Rath <Nikolaus@rath.org>

This program can be distributed under the terms of the GNU GPLv3.
Modified by Jia-Hong Wu on 03/22/2012 to merge previous changes with changes from Ken
'''

from __future__ import division, print_function, absolute_import
from ..common import QuietError, BUFSIZE
from .common import AbstractBucket, NoSuchObject, retry, AuthorizationError, http_connection
from .s3c import HTTPError, BadDigest
from urlparse import urlsplit
import json
import errno
import hashlib
import httplib
import logging
import re
import tempfile
import time
import urllib
from s3ql.backends.common import NoSuchBucket
import socket
socket_timeout = 300
socket.setdefaulttimeout(socket_timeout) 

log = logging.getLogger("backend.swift")

Max_Retries = 100

class Bucket(AbstractBucket):
    """A bucket stored in OpenStack Swift
    
    The bucket guarantees get after create consistency, i.e. a newly created
    object will be immediately retrievable. 
    """

    def __init__(self, storage_url, login, password, use_ssl=True):
        # Unused argument
        #pylint: disable=W0613
        
        super(Bucket, self).__init__()

        (host, port, bucket_name, prefix) = self._parse_storage_url(storage_url)
            
        self.hostname = host
        self.port = port
        self.bucket_name = bucket_name
        self.prefix = prefix
        self.password = password
        self.login = login
        self.auth_token = None
        self.auth_prefix = None
        self.conn = self._get_conn()
        
        self._bucket_exists()
    
    def _bucket_exists(self):
        '''Make sure that the bucket exists'''
        
        try:
            resp = self._do_request('GET', '/', query_string={'limit': 1 })
        except HTTPError as exc:
            if exc.status == 404:
                raise NoSuchBucket(self.bucket_name)
            raise
        resp.read()   
                    
    @staticmethod
    def _parse_storage_url(storage_url):
        '''Extract information from storage URL
        
        Return a tuple *(host, port, bucket_name, prefix)* .
        '''

        hit = re.match(r'^[a-zA-Z0-9]+://' # Backend
                       r'([^/:]+)' # Hostname
                       r'(?::([0-9]+))?' # Port 
                       r'/([^/]+)' # Bucketname
                       r'(?:/(.*))?$', # Prefix
                       storage_url)
        if not hit:
            raise QuietError('Invalid storage URL')

        hostname = hit.group(1)
        port = int(hit.group(2) or '443')
        bucketname = hit.group(3)
        prefix = hit.group(4) or ''
        
        return (hostname, port, bucketname, prefix)

    def is_temp_failure(self, exc): #IGNORE:W0613
        '''Return true if exc indicates a temporary error
    
        Return true if the given exception is used by this bucket's backend
        to indicate a temporary problem. Most instance methods automatically
        retry the request in this case, so the caller does not need to
        worry about temporary failures.
        
        However, in same cases (e.g. when reading or writing an object), the
        request cannot automatically be retried. In these case this method can
        be used to check for temporary problems and so that the request can
        be manually restarted if applicable.
        '''

        if isinstance(exc, (httplib.IncompleteRead,)):
            return True

        # Server closed connection
        elif (isinstance(exc, httplib.BadStatusLine)
              and (not exc.line or exc.line == "''")):
            return True

        elif (isinstance(exc, IOError) and
              exc.errno in (errno.EPIPE, errno.ECONNRESET, errno.ETIMEDOUT,
                            errno.EINTR)):
            return True

        #Added the following lines to deal with temp server error
        elif isinstance(exc, HTTPError) and exc.status >= 500 and exc.status <= 599:
            return True

        elif isinstance(exc, RuntimeError) and exc.status >= 500 and exc.status <= 599:
            return True



        return False
        
    @retry
    def _get_conn(self):
        '''Obtain connection to server and authentication token'''

        log.debug('_get_conn(): start')
        
        conn = http_connection(self.hostname, self.port, ssl=True)
        headers={ 'X-Auth-User': self.login,
                  'X-Auth-Key': self.password }
        
        for auth_path in ('/v1.0', '/auth/v1.0'):
            log.debug('_get_conn(): GET %s', auth_path)
            conn.request('GET', auth_path, None, headers)
            resp = conn.getresponse()
            
            if resp.status == 412:
                log.debug('_refresh_auth(): auth to %s failed, trying next path', auth_path)
                resp.read()
                continue
            
            if resp.status == 401:
                raise AuthorizationError(resp.read())
            
            elif resp.status > 299 or resp.status < 200:
                log.error('_refresh_auth(): unexpected response: %d %s\n%s',
                          resp.status, resp.msg, resp.read())
                raise RuntimeError('Unexpected response: %d %s' % (resp.status,
                                                                   resp.msg))
                
            # Pylint can't infer SplitResult Types
            #pylint: disable=E1103                
            self.auth_token = resp.getheader('X-Auth-Token')
            o = urlsplit(resp.getheader('X-Storage-Url'))
            self.auth_prefix = urllib.unquote(o.path)
            conn.close()

            return http_connection(o.hostname, o.port, ssl=True)
        
        raise RuntimeError('No valid authentication path found')
    
    def _do_request(self, method, path, subres=None, query_string=None,
                    headers=None, body=None):
        '''Send request, read and return response object
        
        This method modifies the *headers* dictionary.
        '''

        log.debug('_do_request(): start with parameters (%r, %r, %r, %r, %r, %r)',
                  method, path, subres, query_string, headers, body)

        if headers is None:
            headers = dict()


        if not body:
            headers['content-length'] = '0'
            
        # Construct full path
        path = urllib.quote('%s/%s%s' % (self.auth_prefix, self.bucket_name, path))
        if query_string:
            s = urllib.urlencode(query_string, doseq=True)
            if subres:
                path += '?%s&%s' % (subres, s)
            else:
                path += '?%s' % s
        elif subres:
            path += '?%s' % subres

        headers['connection'] = 'keep-alive'
        
        num_retries = 0 #Adding maximum retries to do_request
        while num_retries < Max_Retries:
            num_retries = num_retries + 1 

            headers['X-Auth-Token'] = self.auth_token
    
            try:
                log.debug('_do_request(): %s %s', method, path)
                self.conn.request(method, path, body, headers)
    
                log.debug('_do_request(): Reading response..')
                resp = self.conn.getresponse()
            except:
                # We probably can't use the connection anymore
                self.conn.close()
                raise
        
            # We need to call read() at least once for httplib to consider this
            # request finished, even if there is no response body.
            if resp.length == 0:
                resp.read()
    
            # Success 
            if resp.status >= 200 and resp.status <= 299:
                return resp
    
            # Expired auth token
            if resp.status == 401:
                log.info('OpenStack auth token seems to have expired, requesting new one.')
                resp.read()

                #The following three lines added to address 401 error follows by 408 error
                self.conn.close()
                if body:
                    body.seek(0)

                self.conn = self._get_conn()
                continue

            # New section for handling error 408
            if resp.status == 408:
                log.info('Server reports client read timeout. Retrying')
                resp.read()

                if body:
                    body.seek(0)
                continue

            # New section for handling error 499
            if resp.status == 499:
                log.info('Server reports client disconnection. Reconnecting')
                resp.read()

                self.conn.close()
                time.sleep(10)
                if body:
                    body.seek(0)

                self.conn = self._get_conn()
                continue

            # New section for handling server error
            if resp.status >= 500 and resp.status <= 599:
                log.info('Server error. Retrying')
                resp.read()

                time.sleep(10)
            
                if body:
                    body.seek(0)
                continue



            
            # If method == HEAD, server must not return response body
            # even in case of errors
            if method.upper() == 'HEAD':
                raise HTTPError(resp.status, resp.reason)
            else:
                raise HTTPError(resp.status, resp.reason, resp.getheaders(), resp.read())
     
    @retry 
    def lookup(self, key):
        """Return metadata for given key.

        If the key does not exist, `NoSuchObject` is raised.
        """
        log.debug('lookup(%s)', key)

        try:
            resp = self._do_request('HEAD', '/%s%s' % (self.prefix, key))
            assert resp.length == 0
        except HTTPError as exc:
            if exc.status == 404:
                raise NoSuchObject(key)
            else:
                raise

        return extractmeta(resp)

    @retry
    def get_size(self, key):
        '''Return size of object stored under *key*'''

        log.debug('get_size(%s)', key)

        try:
            resp = self._do_request('HEAD', '/%s%s' % (self.prefix, key))
            assert resp.length == 0
        except HTTPError as exc:
            if exc.status == 404:
                raise NoSuchObject(key)
            else:
                raise

        for (name, val) in resp.getheaders():
            if name.lower() == 'content-length':
                return int(val)
        raise RuntimeError('HEAD request did not return Content-Length')
    
    @retry
    def open_read(self, key):
        ''''Open object for reading

        Return a tuple of a file-like object. Bucket contents can be read from
        the file-like object, metadata is stored in its *metadata* attribute and
        can be modified by the caller at will. The object must be closed explicitly.
        '''

        try:
            resp = self._do_request('GET', '/%s%s' % (self.prefix, key))
        except HTTPError as exc:
            if exc.status == 404:
                raise NoSuchObject(key)
            raise

        return ObjectR(key, resp, self, extractmeta(resp))

    def open_write(self, key, metadata=None, is_compressed=False):
        """Open object for writing

        `metadata` can be a dict of additional attributes to store with the
        object. Returns a file-like object. The object must be closed
        explicitly. After closing, the *get_obj_size* may be used to retrieve
        the size of the stored object (which may differ from the size of the
        written data).
        
        The *is_compressed* parameter indicates that the caller is going
        to write compressed data, and may be used to avoid recompression
        by the bucket.
        """
        
        log.debug('open_write(%s): start', key)

        headers = dict()
        if metadata:
            for (hdr, val) in metadata.iteritems():
                headers['X-Object-Meta-%s' % hdr] = val

        return ObjectW(key, self, headers)

    def clear(self):
        """Delete all objects in bucket"""
        
        # We have to cache keys, because otherwise we can't use the
        # http connection to delete keys.
        for (no, s3key) in enumerate(list(self)):
            if no != 0 and no % 1000 == 0:
                log.info('clear(): deleted %d objects so far..', no)

            log.debug('clear(): deleting key %s', s3key)

            # Ignore missing objects when clearing bucket
            self.delete(s3key, True)

    @retry
    def delete(self, key, force=False):
        """Delete object stored under `key`

        ``bucket.delete(key)`` can also be written as ``del bucket[key]``.
        If `force` is true, do not return an error if the key does not exist.
        """

        log.debug('delete(%s)', key)
        try:
            resp = self._do_request('DELETE', '/%s%s' % (self.prefix, key))
            assert resp.length == 0
        except HTTPError as exc:
            if exc.status == 404 and not force:
                #Changed the following two lines to deal with re-deleting files
                log.info('Skip deleting non-existing object %s',key)
                #raise NoSuchObject(key)
            elif exc.status != 404:
                raise

    @retry
    def copy(self, src, dest):
        """Copy data stored under key `src` to key `dest`
        
        If `dest` already exists, it will be overwritten. The copying
        is done on the remote side. 
        """

        log.debug('copy(%s, %s): start', src, dest)

        try:
            resp = self._do_request('PUT', '/%s%s' % (self.prefix, dest),
                                    headers={ 'X-Copy-From': '/%s/%s%s' % (self.bucket_name,
                                                                           self.prefix, src)})
            # Discard response body
            resp.read()
        except HTTPError as exc:
            if exc.status == 404:
                raise NoSuchObject(src)
            raise

    def list(self, prefix=''):
        '''List keys in bucket

        Returns an iterator over all keys in the bucket. This method
        handles temporary errors.
        '''

        log.debug('list(%s): start', prefix)

        marker = ''
        waited = 0
        interval = 1 / 50
        iterator = self._list(prefix, marker)
        while True:
            try:
                marker = iterator.next()
                waited = 0
            except StopIteration:
                break
            except Exception as exc:
                if not self.is_temp_failure(exc):
                    raise
                if waited > 60 * 60:
                    log.error('list(): Timeout exceeded, re-raising %s exception', 
                              type(exc).__name__)
                    raise

                log.info('Encountered %s exception (%s), retrying call to swift.Bucket.list()',
                          type(exc).__name__, exc)
                
                if hasattr(exc, 'retry_after') and exc.retry_after:
                    interval = exc.retry_after
                                    
                time.sleep(interval)
                waited += interval
                interval = min(5*60, 2*interval)
                iterator = self._list(prefix, marker)

            else:
                yield marker

    def _list(self, prefix='', start='', batch_size=5000):
        '''List keys in bucket, starting with *start*

        Returns an iterator over all keys in the bucket. This method
        does not retry on errors.
        '''

        keys_remaining = True
        marker = start
        prefix = self.prefix + prefix
        
        while keys_remaining:
            log.debug('list(%s): requesting with marker=%s', prefix, marker)

            try:
                resp = self._do_request('GET', '/', query_string={'prefix': prefix,
                                                                  'format': 'json',
                                                                  'marker': marker,
                                                                  'limit': batch_size })
            except HTTPError as exc:
                if exc.status == 404:
                    raise NoSuchBucket(self.bucket_name)
                raise
            
            if resp.status == 204:
                return
            
            assert resp.getheader('content-type') == 'application/json; charset=utf-8'
            
            strip = len(self.prefix)
            count = 0
            try:
                for dataset in json.load(resp):
                    count += 1
                    marker = dataset['name'].encode('utf-8')
                    yield marker[strip:]
                
            except GeneratorExit:
                # Need to read rest of response
                while True:
                    buf = resp.read(BUFSIZE)
                    if buf == '':
                        break
                break
            
            keys_remaining = count == batch_size 

            
class ObjectW(object):
    '''A SWIFT object open for writing
    
    All data is first cached in memory, upload only starts when
    the close() method is called.
    '''

    def __init__(self, key, bucket, headers):
        self.key = key
        self.bucket = bucket
        self.headers = headers
        self.closed = False
        self.obj_size = 0
        self.fh = tempfile.TemporaryFile(bufsize=0) # no Python buffering

        # False positive, hashlib *does* have md5 member
        #pylint: disable=E1101        
        self.md5 = hashlib.md5()

    def write(self, buf):
        '''Write object data'''

        self.fh.write(buf)
        self.md5.update(buf)
        self.obj_size += len(buf)

    def is_temp_failure(self, exc):
        return self.bucket.is_temp_failure(exc)

    @retry
    def close(self):
        '''Close object and upload data'''

        # Access to protected member ok
        #pylint: disable=W0212

        log.debug('ObjectW(%s).close(): start', self.key)

        self.closed = True
        self.headers['Content-Length'] = self.obj_size
        self.headers['Content-Type'] = 'application/octet-stream'

        self.fh.seek(0)
        resp = self.bucket._do_request('PUT', '/%s%s' % (self.bucket.prefix, self.key),
                                       headers=self.headers, body=self.fh)
        etag = resp.getheader('ETag').strip('"')
        resp.read()

        if etag != self.md5.hexdigest():
            log.warn('ObjectW(%s).close(): MD5 mismatch (%s vs %s)', self.key, etag,
                     self.md5.hexdigest)
            try:
                self.bucket.delete(self.key)
            except:
                log.exception('Objectw(%s).close(): unable to delete corrupted object!',
                              self.key)            
            raise BadDigest('BadDigest', 'Received ETag does not agree with our calculations.')

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()
        return False

    def get_obj_size(self):
        if not self.closed:
            raise RuntimeError('Object must be closed first.')
        return self.obj_size   
    
class ObjectR(object):
    '''A SWIFT object opened for reading'''

    def __init__(self, key, resp, bucket, metadata=None):
        self.key = key
        self.resp = resp
        self.md5_checked = False
        self.bucket = bucket
        self.metadata = metadata

        # False positive, hashlib *does* have md5 member
        #pylint: disable=E1101        
        self.md5 = hashlib.md5()

    def read(self, size=None):
        '''Read object data
        
        For integrity checking to work, this method has to be called until
        it returns an empty string, indicating that all data has been read
        (and verified).
        '''

        # chunked encoding handled by httplib
        buf = self.resp.read(size)

        # Check MD5 on EOF
        if not buf and not self.md5_checked:
            etag = self.resp.getheader('ETag').strip('"')
            self.md5_checked = True
            if etag != self.md5.hexdigest():
                log.warn('ObjectR(%s).close(): MD5 mismatch: %s vs %s', self.key, etag,
                         self.md5.hexdigest())
                raise BadDigest('BadDigest', 'ETag header does not agree with calculated MD5')
            return buf

        self.md5.update(buf)
        return buf

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def close(self):
        '''Close object'''

        pass    
    
    
def extractmeta(resp):
    '''Extract metadata from HTTP response object'''

    meta = dict()
    for (name, val) in resp.getheaders():
        hit = re.match(r'^X-Object-Meta-(.+)$', name, re.IGNORECASE)
        if not hit:
            continue
        meta[hit.group(1)] = val

    return meta    
