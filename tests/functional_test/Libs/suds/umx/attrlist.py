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
Provides filtered attribute list classes.
"""

from suds import *
from suds.umx import *
from suds.sax import Namespace


class AttrList:
    """
    A filtered attribute list.
    Items are included during iteration if they are in either the (xs) or
    (xml) namespaces.
    @ivar raw: The I{raw} attribute list.
    @type raw: list
    """
    def __init__(self, attributes):
        """
        @param attributes: A list of attributes
        @type attributes: list
        """
        self.raw = attributes

    def real(self):
        """
        Get list of I{real} attributes which exclude xs and xml attributes.
        @return: A list of I{real} attributes.
        @rtype: I{generator}
        """
        for a in self.raw:
            if self.skip(a): continue
            yield a

    def rlen(self):
        """
        Get the number of I{real} attributes which exclude xs and xml attributes.
        @return: A count of I{real} attributes.
        @rtype: L{int}
        """
        n = 0
        for a in self.real():
            n += 1
        return n

    def lang(self):
        """
        Get list of I{filtered} attributes which exclude xs.
        @return: A list of I{filtered} attributes.
        @rtype: I{generator}
        """
        for a in self.raw:
            if a.qname() == 'xml:lang':
                return a.value
            return None

    def skip(self, attr):
        """
        Get whether to skip (filter-out) the specified attribute.
        @param attr: An attribute.
        @type attr: I{Attribute}
        @return: True if should be skipped.
        @rtype: bool
        """
        ns = attr.namespace()
        skip = (
            Namespace.xmlns[1],
            'http://schemas.xmlsoap.org/soap/encoding/',
            'http://schemas.xmlsoap.org/soap/envelope/',
            'http://www.w3.org/2003/05/soap-envelope',
        )
        return ( Namespace.xs(ns) or ns[1] in skip )
