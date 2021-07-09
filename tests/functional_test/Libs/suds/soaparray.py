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
The I{soaparray} module provides XSD extensions for handling
soap (section 5) encoded arrays.
"""

from suds import *
from logging import getLogger
from suds.xsd.sxbasic import Factory as SXFactory
from suds.xsd.sxbasic import Attribute as SXAttribute


class Attribute(SXAttribute):
    """
    Represents an XSD <attribute/> that handles special
    attributes that are extensions for WSDLs.
    @ivar aty: Array type information.
    @type aty: The value of wsdl:arrayType.
    """

    def __init__(self, schema, root, aty):
        """
        @param aty: Array type information.
        @type aty: The value of wsdl:arrayType.
        """
        SXAttribute.__init__(self, schema, root)
        if aty.endswith('[]'):
            self.aty = aty[:-2]
        else:
            self.aty = aty

    def autoqualified(self):
        aqs = SXAttribute.autoqualified(self)
        aqs.append('aty')
        return aqs

    def description(self):
        d = SXAttribute.description(self)
        d = d+('aty',)
        return d

#
# Builder function, only builds Attribute when arrayType
# attribute is defined on root.
#
def __fn(x, y):
    ns = (None, "http://schemas.xmlsoap.org/wsdl/")
    aty = y.get('arrayType', ns=ns)
    if aty is None:
        return SXAttribute(x, y)
    return Attribute(x, y, aty)

#
# Remap <xs:attribute/> tags to __fn() builder.
#
SXFactory.maptag('attribute', __fn)
