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
The I{builder} module provides an wsdl/xsd defined types factory
"""

from logging import getLogger
from suds import *
from suds.sudsobject import Factory

log = getLogger(__name__)


class Builder:
    """ Builder used to construct an object for types defined in the schema """

    def __init__(self, resolver):
        """
        @param resolver: A schema object name resolver.
        @type resolver: L{resolver.Resolver}
        """
        self.resolver = resolver

    def build(self, name):
        """ build a an object for the specified typename as defined in the schema """
        if isinstance(name, basestring):
            type = self.resolver.find(name)
            if type is None:
                raise TypeNotFound(name)
        else:
            type = name
        cls = type.name
        if type.mixed():
            data = Factory.property(cls)
        else:
            data = Factory.object(cls)
        resolved = type.resolve()
        md = data.__metadata__
        md.sxtype = resolved
        md.ordering = self.ordering(resolved)
        history = []
        self.add_attributes(data, resolved)
        for child, ancestry in type.children():
            if self.skip_child(child, ancestry):
                continue
            self.process(data, child, history[:])
        return data

    def process(self, data, type, history):
        """ process the specified type then process its children """
        if type in history:
            return
        if type.enum():
            return
        history.append(type)
        resolved = type.resolve()
        value = None
        if type.multi_occurrence():
            value = []
        else:
            if len(resolved) > 0:
                if resolved.mixed():
                    value = Factory.property(resolved.name)
                    md = value.__metadata__
                    md.sxtype = resolved
                else:
                    value = Factory.object(resolved.name)
                    md = value.__metadata__
                    md.sxtype = resolved
                    md.ordering = self.ordering(resolved)
        setattr(data, type.name, value)
        if value is not None:
            data = value
        if not isinstance(data, list):
            self.add_attributes(data, resolved)
            for child, ancestry in resolved.children():
                if self.skip_child(child, ancestry):
                    continue
                self.process(data, child, history[:])

    def add_attributes(self, data, type):
        """ add required attributes """
        for attr, ancestry in type.attributes():
            name = '_%s' % attr.name
            value = attr.get_default()
            setattr(data, name, value)

    def skip_child(self, child, ancestry):
        """ get whether or not to skip the specified child """
        if child.any(): return True
        for x in ancestry:
            if x.choice():
                return True
        return False

    def ordering(self, type):
        """ get the ordering """
        result = []
        for child, ancestry in type.resolve():
            name = child.name
            if child.name is None:
                continue
            if child.isattr():
                name = '_%s' % child.name
            result.append(name)
        return result
