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
The I{sxbuiltin} module provides classes that represent
XSD I{builtin} schema objects.
"""

from suds import *
from suds.xsd import *
from suds.sax.date import *
from suds.xsd.sxbase import XBuiltin

import datetime as dt
from logging import getLogger

log = getLogger(__name__)


class XString(XBuiltin):
    """
    Represents an (xsd) <xs:string/> node
    """
    pass


class XAny(XBuiltin):
    """
    Represents an (xsd) <any/> node
    """

    def __init__(self, schema, name):
        XBuiltin.__init__(self, schema, name)
        self.nillable = False

    def get_child(self, name):
        child = XAny(self.schema, name)
        return child, []

    def any(self):
        return True


class XBoolean(XBuiltin):
    """
    Represents an (xsd) boolean builtin type.
    """

    translation = ({'1':True, 'true':True, '0':False, 'false':False},
        {True:'true', 1:'true', False:'false', 0:'false'})

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring):
                return XBoolean.translation[0].get(value)
        else:
            if isinstance(value, (bool, int)):
                return XBoolean.translation[1].get(value)
            return value


class XInteger(XBuiltin):
    """
    Represents an (xsd) xs:int builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return int(value)
        else:
            if isinstance(value, int):
                return str(value)
            return value


class XLong(XBuiltin):
    """
    Represents an (xsd) xs:long builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return long(value)
        else:
            if isinstance(value, (int, long)):
                return str(value)
            return value


class XFloat(XBuiltin):
    """
    Represents an (xsd) xs:float builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return float(value)
        else:
            if isinstance(value, float):
                return str(value)
            return value


class XDate(XBuiltin):
    """
    Represents an (xsd) xs:date builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return Date(value).value
        else:
            if isinstance(value, dt.date):
                return str(Date(value))
            return value


class XTime(XBuiltin):
    """
    Represents an (xsd) xs:time builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return Time(value).value
        else:
            if isinstance(value, dt.time):
                return str(Time(value))
            return value


class XDateTime(XBuiltin):
    """
    Represents an (xsd) xs:datetime builtin type.
    """

    @staticmethod
    def translate(value, topython=True):
        if topython:
            if isinstance(value, basestring) and len(value):
                return DateTime(value).value
        else:
            if isinstance(value, dt.datetime):
                return str(DateTime(value))
            return value


class Factory:

    tags =\
    {
        # any
        'anyType' : XAny,
        # strings
        'string' : XString,
        'normalizedString' : XString,
        'ID' : XString,
        'Name' : XString,
        'QName' : XString,
        'NCName' : XString,
        'anySimpleType' : XString,
        'anyURI' : XString,
        'NOTATION' : XString,
        'token' : XString,
        'language' : XString,
        'IDREFS' : XString,
        'ENTITIES' : XString,
        'IDREF' : XString,
        'ENTITY' : XString,
        'NMTOKEN' : XString,
        'NMTOKENS' : XString,
        # binary
        'hexBinary' : XString,
        'base64Binary' : XString,
        # integers
        'int' : XInteger,
        'integer' : XInteger,
        'unsignedInt' : XInteger,
        'positiveInteger' : XInteger,
        'negativeInteger' : XInteger,
        'nonPositiveInteger' : XInteger,
        'nonNegativeInteger' : XInteger,
        # longs
        'long' : XLong,
        'unsignedLong' : XLong,
        # shorts
        'short' : XInteger,
        'unsignedShort' : XInteger,
        'byte' : XInteger,
        'unsignedByte' : XInteger,
        # floats
        'float' : XFloat,
        'double' : XFloat,
        'decimal' : XFloat,
        # dates & times
        'date' : XDate,
        'time' : XTime,
        'dateTime': XDateTime,
        'duration': XString,
        'gYearMonth' : XString,
        'gYear' : XString,
        'gMonthDay' : XString,
        'gDay' : XString,
        'gMonth' : XString,
        # boolean
        'boolean' : XBoolean,
    }

    @classmethod
    def maptag(cls, tag, fn):
        """
        Map (override) tag => I{class} mapping.
        @param tag: An xsd tag name.
        @type tag: str
        @param fn: A function or class.
        @type fn: fn|class.
        """
        cls.tags[tag] = fn

    @classmethod
    def create(cls, schema, name):
        """
        Create an object based on the root tag name.
        @param schema: A schema object.
        @type schema: L{schema.Schema}
        @param name: The name.
        @type name: str
        @return: The created object.
        @rtype: L{XBuiltin}
        """
        fn = cls.tags.get(name)
        if fn is not None:
            return fn(schema, name)
        return XBuiltin(schema, name)
