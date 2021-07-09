#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Contains basic caching classes.
"""

import suds
from suds.transport import *
from suds.sax.parser import Parser
from suds.sax.element import Element

from datetime import datetime as dt
from datetime import timedelta
from logging import getLogger
import os
from tempfile import gettempdir as tmp
try:
    import cPickle as pickle
except Exception:
    import pickle

log = getLogger(__name__)


class Cache:
    """
    An object object cache.
    """

    def get(self, id):
        """
        Get a object from the cache by ID.
        @param id: The object ID.
        @type id: str
        @return: The object, else None
        @rtype: any
        """
        raise Exception('not-implemented')

    def put(self, id, object):
        """
        Put a object into the cache.
        @param id: The object ID.
        @type id: str
        @param object: The object to add.
        @type object: any
        """
        raise Exception('not-implemented')

    def purge(self, id):
        """
        Purge a object from the cache by id.
        @param id: A object ID.
        @type id: str
        """
        raise Exception('not-implemented')

    def clear(self):
        """
        Clear all objects from the cache.
        """
        raise Exception('not-implemented')


class NoCache(Cache):
    """
    The passthru object cache.
    """

    def get(self, id):
        return None

    def put(self, id, object):
        pass


class FileCache(Cache):
    """
    A file-based URL cache.
    @cvar fnprefix: The file name prefix.
    @type fnsuffix: str
    @ivar duration: The cached file duration which defines how
        long the file will be cached.
    @type duration: (unit, value)
    @ivar location: The directory for the cached files.
    @type location: str
    """
    fnprefix = 'suds'
    units = ('months', 'weeks', 'days', 'hours', 'minutes', 'seconds')

    def __init__(self, location=None, **duration):
        """
        @param location: The directory for the cached files.
        @type location: str
        @param duration: The cached file duration which defines how
            long the file will be cached.  A duration=0 means forever.
            The duration may be: (months|weeks|days|hours|minutes|seconds).
        @type duration: {unit:value}
        """
        if location is None:
            location = os.path.join(tmp(), 'suds')
        self.location = location
        self.duration = (None, 0)
        self.setduration(**duration)
        self.checkversion()

    def fnsuffix(self):
        """
        Get the file name suffix
        @return: The suffix
        @rtype: str
        """
        return 'gcf'

    def setduration(self, **duration):
        """
        Set the caching duration which defines how long the
        file will be cached.
        @param duration: The cached file duration which defines how
            long the file will be cached.  A duration=0 means forever.
            The duration may be: (months|weeks|days|hours|minutes|seconds).
        @type duration: {unit:value}
        """
        if len(duration) == 1:
            arg = duration.items()[0]
            if not arg[0] in self.units:
                raise Exception('must be: %s' % str(self.units))
            self.duration = arg
        return self

    def setlocation(self, location):
        """
        Set the location (directory) for the cached files.
        @param location: The directory for the cached files.
        @type location: str
        """
        self.location = location

    def mktmp(self):
        """
        Make the I{location} directory if it doesn't already exits.
        """
        try:
            if not os.path.isdir(self.location):
                os.makedirs(self.location)
        except Exception:
            log.debug(self.location, exc_info=1)
        return self

    def put(self, id, bfr):
        try:
            fn = self.__fn(id)
            f = self.open(fn, 'wb')
            try:
                f.write(bfr)
            finally:
                f.close()
            return bfr
        except Exception:
            log.debug(id, exc_info=1)
            return bfr

    def get(self, id):
        try:
            f = self.getf(id)
            try:
                return f.read()
            finally:
                f.close()
        except Exception:
            pass

    def getf(self, id):
        try:
            fn = self.__fn(id)
            self.validate(fn)
            return self.open(fn, 'rb')
        except Exception:
            pass

    def validate(self, fn):
        """
        Validate that the file has not expired based on the I{duration}.
        @param fn: The file name.
        @type fn: str
        """
        if self.duration[1] < 1:
            return
        created = dt.fromtimestamp(os.path.getctime(fn))
        d = {self.duration[0]:self.duration[1]}
        expired = created + timedelta(**d)
        if expired < dt.now():
            log.debug('%s expired, deleted', fn)
            os.remove(fn)

    def clear(self):
        for fn in os.listdir(self.location):
            path = os.path.join(self.location, fn)
            if os.path.isdir(path):
                continue
            if fn.startswith(self.fnprefix):
                os.remove(path)
                log.debug('deleted: %s', path)

    def purge(self, id):
        fn = self.__fn(id)
        try:
            os.remove(fn)
        except Exception:
            pass

    def open(self, fn, *args):
        """
        Open the cache file making sure the directory is created.
        """
        self.mktmp()
        return open(fn, *args)

    def checkversion(self):
        path = os.path.join(self.location, 'version')
        try:
            f = self.open(path)
            version = f.read()
            f.close()
            if version != suds.__version__:
                raise Exception()
        except Exception:
            self.clear()
            f = self.open(path, 'w')
            f.write(suds.__version__)
            f.close()

    def __fn(self, id):
        name = id
        suffix = self.fnsuffix()
        fn = '%s-%s.%s' % (self.fnprefix, name, suffix)
        return os.path.join(self.location, fn)


class DocumentCache(FileCache):
    """
    Provides xml document caching.
    """

    def fnsuffix(self):
        return 'xml'

    def get(self, id):
        try:
            fp = self.getf(id)
            if fp is None:
                return None
            p = Parser()
            return p.parse(fp)
        except Exception:
            self.purge(id)

    def put(self, id, object):
        if isinstance(object, Element):
            FileCache.put(self, id, suds.byte_str(str(object)))
        return object


class ObjectCache(FileCache):
    """
    Provides pickled object caching.
    @cvar protocol: The pickling protocol.
    @type protocol: int
    """
    protocol = 2

    def fnsuffix(self):
        return 'px'

    def get(self, id):
        try:
            fp = self.getf(id)
            if fp is None:
                return None
            return pickle.load(fp)
        except Exception:
            self.purge(id)

    def put(self, id, object):
        bfr = pickle.dumps(object, self.protocol)
        FileCache.put(self, id, bfr)
        return object
