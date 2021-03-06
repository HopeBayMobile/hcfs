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
Contains XML text classes.
"""

from suds import *
from suds.sax import *


class Text(unicode):
    """
    An XML text object used to represent text content.
    @ivar lang: The (optional) language flag.
    @type lang: bool
    @ivar escaped: The (optional) XML special character escaped flag.
    @type escaped: bool
    """
    __slots__ = ('lang', 'escaped')

    @classmethod
    def __valid(cls, *args):
        return len(args) and args[0] is not None

    def __new__(cls, *args, **kwargs):
        if cls.__valid(*args):
            lang = kwargs.pop('lang', None)
            escaped = kwargs.pop('escaped', False)
            result = super(Text, cls).__new__(cls, *args, **kwargs)
            result.lang = lang
            result.escaped = escaped
        else:
            result = None
        return result

    def escape(self):
        """
        Encode (escape) special XML characters.
        @return: The text with XML special characters escaped.
        @rtype: L{Text}
        """
        if not self.escaped:
            post = sax.encoder.encode(self)
            escaped = ( post != self )
            return Text(post, lang=self.lang, escaped=escaped)
        return self

    def unescape(self):
        """
        Decode (unescape) special XML characters.
        @return: The text with escaped XML special characters decoded.
        @rtype: L{Text}
        """
        if self.escaped:
            post = sax.encoder.decode(self)
            return Text(post, lang=self.lang)
        return self

    def trim(self):
        post = self.strip()
        return Text(post, lang=self.lang, escaped=self.escaped)

    def __add__(self, other):
        joined = u''.join((self, other))
        result = Text(joined, lang=self.lang, escaped=self.escaped)
        if isinstance(other, Text):
            result.escaped = self.escaped or other.escaped
        return result

    def __repr__(self):
        s = [self]
        if self.lang is not None:
            s.append(' [%s]' % self.lang)
        if self.escaped:
            s.append(' <escaped>')
        return ''.join(s)

    def __getstate__(self):
        state = {}
        for k in self.__slots__:
            state[k] = getattr(self, k)
        return state

    def __setstate__(self, state):
        for k in self.__slots__:
            setattr(self, k, state[k])


class Raw(Text):
    """
    Raw text which is not XML escaped.
    This may include I{string} XML.
    """
    def escape(self):
        return self

    def unescape(self):
        return self

    def __add__(self, other):
        joined = u''.join((self, other))
        return Raw(joined, lang=self.lang)
