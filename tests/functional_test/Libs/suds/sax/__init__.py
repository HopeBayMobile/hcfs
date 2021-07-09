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
The sax module contains a collection of classes that provide a (D)ocument
(O)bject (M)odel representation of an XML document. The goal is to provide an
easy, intuative interface for managing XML documents. Although, the term, DOM,
is used above, this model is B{far} better.

XML namespaces in suds are represented using a (2) element tuple containing the
prefix and the URI, e.g. I{('tns', 'http://myns')}

@var encoder: A I{pluggable} XML special character processor used to encode/
    decode strings.
@type encoder: L{Encoder}
"""

from suds.sax.enc import Encoder

#
# pluggable XML special character encoder.
#
encoder = Encoder()


def splitPrefix(name):
    """
    Split the name into a tuple (I{prefix}, I{name}). The first element in the
    tuple is I{None} when the name does not have a prefix.
    @param name: A node name containing an optional prefix.
    @type name: basestring
    @return: A tuple containing the (2) parts of I{name}
    @rtype: (I{prefix}, I{name})
    """
    if isinstance(name, basestring) and ':' in name:
        return tuple(name.split(':', 1))
    return None, name


class Namespace:
    """
    The namespace class represents XML namespaces.
    """

    default = (None, None)
    xmlns = ('xml', 'http://www.w3.org/XML/1998/namespace')
    xsdns = ('xs', 'http://www.w3.org/2001/XMLSchema')
    xsins = ('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    all = (xsdns, xsins)

    @classmethod
    def create(cls, p=None, u=None):
        return p, u

    @classmethod
    def none(cls, ns):
        return ns == cls.default

    @classmethod
    def xsd(cls, ns):
        try:
            return cls.w3(ns) and ns[1].endswith('XMLSchema')
        except:
            pass
        return False

    @classmethod
    def xsi(cls, ns):
        try:
            return cls.w3(ns) and ns[1].endswith('XMLSchema-instance')
        except:
            pass
        return False

    @classmethod
    def xs(cls, ns):
        return cls.xsd(ns) or cls.xsi(ns)

    @classmethod
    def w3(cls, ns):
        try:
            return ns[1].startswith('http://www.w3.org')
        except:
            pass
        return False

    @classmethod
    def isns(cls, ns):
        try:
            return isinstance(ns, tuple) and len(ns) == len(cls.default)
        except:
            pass
        return False
