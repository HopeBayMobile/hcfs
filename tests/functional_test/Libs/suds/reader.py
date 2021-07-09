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
  XML document reader classes providing integration with the suds library's
caching system.
"""


from suds.sax.parser import Parser
from suds.transport import Request
from suds.cache import Cache, NoCache
from suds.store import DocumentStore
from suds.plugin import PluginContainer
from logging import getLogger


log = getLogger(__name__)


class Reader:
    """
    Provides integration with the cache.
    @ivar options: An options object.
    @type options: I{Options}
    """

    def __init__(self, options):
        """
        @param options: An options object.
        @type options: I{Options}
        """
        self.options = options
        self.plugins = PluginContainer(options.plugins)

    def mangle(self, name, x):
        """
        Mangle the name by hashing the I{name} and appending I{x}.
        @return: the mangled name.
        """
        h = abs(hash(name))
        return '%s-%s' % (h, x)


class DocumentReader(Reader):
    """
    Provides integration between the SAX L{Parser} and the document cache.
    """

    def open(self, url):
        """
        Open an XML document at the specified I{URL}.
        First, the document attempted to be retrieved from the I{object cache}.
        If not found, it is downloaded and parsed using the SAX parser. The
        result is added to the cache for the next open().
        @param url: A document URL.
        @type url: str.
        @return: The specified XML document.
        @rtype: I{Document}
        """
        cache = self.cache()
        id = self.mangle(url, 'document')
        d = cache.get(id)
        if d is None:
            d = self.download(url)
            cache.put(id, d)
        self.plugins.document.parsed(url=url, document=d.root())
        return d

    def download(self, url):
        """
        Download the document.
        @param url: A document URL.
        @type url: str.
        @return: A file pointer to the document.
        @rtype: file-like
        """
        content = None
        store = self.options.documentStore
        if store is not None:
            content = store.open(url)
        if content is None:
            fp = self.options.transport.open(Request(url))
            try:
                content = fp.read()
            finally:
                fp.close()
        ctx = self.plugins.document.loaded(url=url, document=content)
        content = ctx.document
        sax = Parser()
        return sax.parse(string=content)

    def cache(self):
        """
        Get the cache.
        @return: The I{cache} when I{cachingpolicy} = B{0}.
        @rtype: L{Cache}
        """
        if self.options.cachingpolicy == 0:
            return self.options.cache
        return NoCache()


class DefinitionsReader(Reader):
    """
    Provides integration between the WSDL Definitions object and the object
    cache.
    @ivar fn: A factory function (constructor) used to
        create the object not found in the cache.
    @type fn: I{Constructor}
    """

    def __init__(self, options, fn):
        """
        @param options: An options object.
        @type options: I{Options}
        @param fn: A factory function (constructor) used to create the object
            not found in the cache.
        @type fn: I{Constructor}
        """
        Reader.__init__(self, options)
        self.fn = fn

    def open(self, url):
        """
        Open a WSDL at the specified I{URL}.
        First, the WSDL attempted to be retrieved from
        the I{object cache}.  After unpickled from the cache, the
        I{options} attribute is restored.
        If not found, it is downloaded and instantiated using the
        I{fn} constructor and added to the cache for the next open().
        @param url: A WSDL URL.
        @type url: str.
        @return: The WSDL object.
        @rtype: I{Definitions}
        """
        cache = self.cache()
        id = self.mangle(url, 'wsdl')
        d = cache.get(id)
        if d is None:
            d = self.fn(url, self.options)
            cache.put(id, d)
        else:
            d.options = self.options
            for imp in d.imports:
                imp.imported.options = self.options
        return d

    def cache(self):
        """
        Get the cache.
        @return: The I{cache} when I{cachingpolicy} = B{1}.
        @rtype: L{Cache}
        """
        if self.options.cachingpolicy == 1:
            return self.options.cache
        return NoCache()
