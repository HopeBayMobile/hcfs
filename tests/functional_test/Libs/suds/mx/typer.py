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
Provides sx typing classes.
"""

from logging import getLogger
from suds import *
from suds.mx import *
from suds.sax import Namespace as NS
from suds.sax.text import Text

log = getLogger(__name__)


class Typer:
    """
    Provides XML node typing as either automatic or manual.
    @cvar types:  A dict of class to xs type mapping.
    @type types: dict
    """

    types = {
        int : ('int', NS.xsdns),
        long : ('long', NS.xsdns),
        float : ('float', NS.xsdns),
        str : ('string', NS.xsdns),
        unicode : ('string', NS.xsdns),
        Text : ('string', NS.xsdns),
        bool : ('boolean', NS.xsdns),
     }

    @classmethod
    def auto(cls, node, value=None):
        """
        Automatically set the node's xsi:type attribute based on either I{value}'s
        class or the class of the node's text.  When I{value} is an unmapped class,
        the default type (xs:any) is set.
        @param node: An XML node
        @type node: L{sax.element.Element}
        @param value: An object that is or would be the node's text.
        @type value: I{any}
        @return: The specified node.
        @rtype: L{sax.element.Element}
        """
        if value is None:
            value = node.getText()
        if isinstance(value, Object):
            known = cls.known(value)
            if known.name is None:
                return node
            tm = (known.name, known.namespace())
        else:
            tm = cls.types.get(value.__class__, cls.types.get(str))
        cls.manual(node, *tm)
        return node

    @classmethod
    def manual(cls, node, tval, ns=None):
        """
        Set the node's xsi:type attribute based on either I{value}'s
        class or the class of the node's text.  Then adds the referenced
        prefix(s) to the node's prefix mapping.
        @param node: An XML node
        @type node: L{sax.element.Element}
        @param tval: The name of the schema type.
        @type tval: str
        @param ns: The XML namespace of I{tval}.
        @type ns: (prefix, uri)
        @return: The specified node.
        @rtype: L{sax.element.Element}
        """
        xta = ':'.join((NS.xsins[0], 'type'))
        node.addPrefix(NS.xsins[0], NS.xsins[1])
        if ns is None:
            node.set(xta, tval)
        else:
            ns = cls.genprefix(node, ns)
            qname = ':'.join((ns[0], tval))
            node.set(xta, qname)
            node.addPrefix(ns[0], ns[1])
        return node

    @classmethod
    def genprefix(cls, node, ns):
        """
        Generate a prefix.
        @param node: An XML node on which the prefix will be used.
        @type node: L{sax.element.Element}
        @param ns: A namespace needing an unique prefix.
        @type ns: (prefix, uri)
        @return: The I{ns} with a new prefix.
        """
        for n in range(1, 1024):
            p = 'ns%d' % n
            u = node.resolvePrefix(p, default=None)
            if u is None or u == ns[1]:
                return (p, ns[1])
        raise Exception('auto prefix, exhausted')

    @classmethod
    def known(cls, object):
        try:
            md = object.__metadata__
            known = md.sxtype
            return known
        except:
            pass
