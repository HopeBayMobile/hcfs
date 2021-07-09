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
Provides base classes for XML->object I{unmarshalling}.
"""

from logging import getLogger
from suds import *
from suds.umx import *
from suds.umx.attrlist import AttrList
from suds.sax.text import Text
from suds.sudsobject import Factory, merge


log = getLogger(__name__)

reserved = { 'class':'cls', 'def':'dfn', }

class Core:
    """
    The abstract XML I{node} unmarshaller.  This class provides the
    I{core} unmarshalling functionality.
    """

    def process(self, content):
        """
        Process an object graph representation of the xml I{node}.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: A suds object.
        @rtype: L{Object}
        """
        self.reset()
        return self.append(content)

    def append(self, content):
        """
        Process the specified node and convert the XML document into
        a I{suds} L{object}.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: A I{append-result} tuple as: (L{Object}, I{value})
        @rtype: I{append-result}
        @note: This is not the proper entry point.
        @see: L{process()}
        """
        self.start(content)
        self.append_attributes(content)
        self.append_children(content)
        self.append_text(content)
        self.end(content)
        return self.postprocess(content)

    def postprocess(self, content):
        """
        Perform final processing of the resulting data structure as follows:
          - Mixed values (children and text) will have a result of the I{content.node}.
          - Simi-simple values (attributes, no-children and text) will have a result of a
             property object.
          - Simple values (no-attributes, no-children with text nodes) will have a string
             result equal to the value of the content.node.getText().
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: The post-processed result.
        @rtype: I{any}
        """
        node = content.node
        if len(node.children) and node.hasText():
            return node
        attributes = AttrList(node.attributes)
        if attributes.rlen() and \
            not len(node.children) and \
            node.hasText():
                p = Factory.property(node.name, node.getText())
                return merge(content.data, p)
        if len(content.data):
            return content.data
        lang = attributes.lang()
        if content.node.isnil():
            return None
        if not len(node.children) and content.text is None:
            if self.nillable(content):
                return None
            else:
                return Text('', lang=lang)
        if isinstance(content.text, basestring):
            return Text(content.text, lang=lang)
        else:
            return content.text

    def append_attributes(self, content):
        """
        Append attribute nodes into L{Content.data}.
        Attributes in the I{schema} or I{xml} namespaces are skipped.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        """
        attributes = AttrList(content.node.attributes)
        for attr in attributes.real():
            name = attr.name
            value = attr.value
            self.append_attribute(name, value, content)

    def append_attribute(self, name, value, content):
        """
        Append an attribute name/value into L{Content.data}.
        @param name: The attribute name
        @type name: basestring
        @param value: The attribute's value
        @type value: basestring
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        """
        key = name
        key = '_%s' % reserved.get(key, key)
        setattr(content.data, key, value)

    def append_children(self, content):
        """
        Append child nodes into L{Content.data}
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        """
        for child in content.node:
            cont = Content(child)
            cval = self.append(cont)
            key = reserved.get(child.name, child.name)
            if key in content.data:
                v = getattr(content.data, key)
                if isinstance(v, list):
                    v.append(cval)
                else:
                    setattr(content.data, key, [v, cval])
                continue
            if self.multi_occurrence(cont):
                if cval is None:
                    setattr(content.data, key, [])
                else:
                    setattr(content.data, key, [cval,])
            else:
                setattr(content.data, key, cval)

    def append_text(self, content):
        """
        Append text nodes into L{Content.data}
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        """
        if content.node.hasText():
            content.text = content.node.getText()

    def reset(self):
        pass

    def start(self, content):
        """
        Processing on I{node} has started.  Build and return
        the proper object.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: A subclass of Object.
        @rtype: L{Object}
        """
        content.data = Factory.object(content.node.name)

    def end(self, content):
        """
        Processing on I{node} has ended.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        """
        pass

    def single_occurrence(self, content):
        """
        Get whether the content has at most a single occurrence (not a list).
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: True if content has at most a single occurrence, else False.
        @rtype: boolean
        '"""
        return not self.multi_occurrence(content)

    def multi_occurrence(self, content):
        """
        Get whether the content has more than one occurrence (a list).
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: True if content has more than one occurrence, else False.
        @rtype: boolean
        '"""
        return False

    def nillable(self, content):
        """
        Get whether the object is nillable.
        @param content: The current content being unmarshalled.
        @type content: L{Content}
        @return: True if nillable, else False
        @rtype: boolean
        '"""
        return False
