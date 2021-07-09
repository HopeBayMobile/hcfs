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
The sax module contains a collection of classes that provide a
(D)ocument (O)bject (M)odel representation of an XML document.
The goal is to provide an easy, intuative interface for managing XML
documents.  Although, the term, DOM, is used above, this model is
B{far} better.

XML namespaces in suds are represented using a (2) element tuple
containing the prefix and the URI.  Eg: I{('tns', 'http://myns')}

"""

import suds
from suds import *
from suds.sax import *
from suds.sax.attribute import Attribute
from suds.sax.document import Document
from suds.sax.element import Element
from suds.sax.text import Text

from logging import getLogger
import sys
from xml.sax import make_parser, InputSource, ContentHandler
from xml.sax.handler import feature_external_ges


log = getLogger(__name__)


class Handler(ContentHandler):
    """ sax hanlder """

    def __init__(self):
        self.nodes = [Document()]

    def startElement(self, name, attrs):
        top = self.top()
        node = Element(unicode(name))
        for a in attrs.getNames():
            n = unicode(a)
            v = unicode(attrs.getValue(a))
            attribute = Attribute(n,v)
            if self.mapPrefix(node, attribute):
                continue
            node.append(attribute)
        node.charbuffer = []
        top.append(node)
        self.push(node)

    def mapPrefix(self, node, attribute):
        skip = False
        if attribute.name == 'xmlns':
            if len(attribute.value):
                node.expns = unicode(attribute.value)
            skip = True
        elif attribute.prefix == 'xmlns':
            prefix = attribute.name
            node.nsprefixes[prefix] = unicode(attribute.value)
            skip = True
        return skip

    def endElement(self, name):
        name = unicode(name)
        current = self.top()
        if len(current.charbuffer):
            current.text = Text(u''.join(current.charbuffer))
        del current.charbuffer
        if len(current):
            current.trim()
        if name == current.qname():
            self.pop()
        else:
            raise Exception('malformed document')

    def characters(self, content):
        text = unicode(content)
        node = self.top()
        node.charbuffer.append(text)

    def push(self, node):
        self.nodes.append(node)
        return node

    def pop(self):
        return self.nodes.pop()

    def top(self):
        return self.nodes[len(self.nodes)-1]


class Parser:
    """ SAX Parser """

    @classmethod
    def saxparser(cls):
        p = make_parser()
        p.setFeature(feature_external_ges, 0)
        h = Handler()
        p.setContentHandler(h)
        return (p, h)

    def parse(self, file=None, string=None):
        """
        SAX parse XML text.
        @param file: Parse a python I{file-like} object.
        @type file: I{file-like} object.
        @param string: Parse string XML.
        @type string: str
        """
        timer = suds.metrics.Timer()
        timer.start()
        sax, handler = self.saxparser()
        if file is not None:
            sax.parse(file)
            timer.stop()
            suds.metrics.log.debug('sax (%s) duration: %s', file, timer)
            return handler.nodes[0]
        if string is not None:
            source = InputSource(None)
            source.setByteStream(suds.BytesIO(string))
            sax.parse(source)
            timer.stop()
            suds.metrics.log.debug('%s\nsax duration: %s', string, timer)
            return handler.nodes[0]
