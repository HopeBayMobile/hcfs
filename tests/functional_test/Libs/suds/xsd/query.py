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
The I{query} module defines a class for performing schema queries.
"""

from suds import *
from suds.sudsobject import *
from suds.xsd import qualify, isqref
from suds.xsd.sxbuiltin import Factory

from logging import getLogger

log = getLogger(__name__)


class Query(Object):
    """
    Schema query base class.

    """
    def __init__(self, ref=None):
        """
        @param ref: The schema reference being queried.
        @type ref: qref
        """
        Object.__init__(self)
        self.id = objid(self)
        self.ref = ref
        self.history = []
        self.resolved = False
        if not isqref(self.ref):
            raise Exception('%s, must be qref' % tostr(self.ref))

    def execute(self, schema):
        """
        Execute this query using the specified schema.
        @param schema: The schema associated with the query. The schema is used
            by the query to search for items.
        @type schema: L{schema.Schema}
        @return: The item matching the search criteria.
        @rtype: L{sxbase.SchemaObject}
        """
        raise Exception, 'not-implemented by subclass'

    def filter(self, result):
        """
        Filter the specified result based on query criteria.
        @param result: A potential result.
        @type result: L{sxbase.SchemaObject}
        @return: True if result should be excluded.
        @rtype: boolean
        """
        if result is None:
            return True
        reject = ( result in self.history )
        if reject:
            log.debug('result %s, rejected by\n%s', Repr(result), self)
        return reject

    def result(self, result):
        """
        Query result post processing.
        @param result: A query result.
        @type result: L{sxbase.SchemaObject}
        """
        if result is None:
            log.debug('%s, not-found', self.ref)
            return
        if self.resolved:
            result = result.resolve()
        log.debug('%s, found as: %s', self.ref, Repr(result))
        self.history.append(result)
        return result


class BlindQuery(Query):
    """
    Schema query class that I{blindly} searches for a reference in the
    specified schema. It may be used to find Elements and Types but will match
    on an Element first. This query will also find builtins.

    """
    def execute(self, schema):
        if schema.builtin(self.ref):
            name = self.ref[0]
            b = Factory.create(schema, name)
            log.debug('%s, found builtin (%s)', self.id, name)
            return b
        result = None
        for d in (schema.elements, schema.types):
            result = d.get(self.ref)
            if self.filter(result):
                result = None
            else:
                break
        if result is None:
            eq = ElementQuery(self.ref)
            eq.history = self.history
            result = eq.execute(schema)
        return self.result(result)


class TypeQuery(Query):
    """
    Schema query class that searches for Type references in the specified
    schema. Matches on root types only.

    """
    def execute(self, schema):
        if schema.builtin(self.ref):
            name = self.ref[0]
            b = Factory.create(schema, name)
            log.debug('%s, found builtin (%s)', self.id, name)
            return b
        result = schema.types.get(self.ref)
        if self.filter(result):
            result = None
        return self.result(result)


class GroupQuery(Query):
    """
    Schema query class that searches for Group references in the specified
    schema.

    """
    def execute(self, schema):
        result = schema.groups.get(self.ref)
        if self.filter(result):
            result = None
        return self.result(result)


class AttrQuery(Query):
    """
    Schema query class that searches for Attribute references in the specified
    schema. Matches on root Attribute by qname first, then searches deeper into
    the document.

    """
    def execute(self, schema):
        result = schema.attributes.get(self.ref)
        if self.filter(result):
            result = self.__deepsearch(schema)
        return self.result(result)

    def __deepsearch(self, schema):
        from suds.xsd.sxbasic import Attribute
        result = None
        for e in schema.all:
            result = e.find(self.ref, (Attribute,))
            if self.filter(result):
                result = None
            else:
                break
        return result


class AttrGroupQuery(Query):
    """
    Schema query class that searches for attributeGroup references in the
    specified schema.

    """
    def execute(self, schema):
        result = schema.agrps.get(self.ref)
        if self.filter(result):
            result = None
        return self.result(result)


class ElementQuery(Query):
    """
    Schema query class that searches for Element references in the specified
    schema. Matches on root Elements by qname first, then searches deeper into
    the document.

    """
    def execute(self, schema):
        result = schema.elements.get(self.ref)
        if self.filter(result):
            result = self.__deepsearch(schema)
        return self.result(result)

    def __deepsearch(self, schema):
        from suds.xsd.sxbasic import Element
        result = None
        for e in schema.all:
            result = e.find(self.ref, (Element,))
            if self.filter(result):
                result = None
            else:
                break
        return result
