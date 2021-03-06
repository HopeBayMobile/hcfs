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
Provides classes for the (WS) SOAP I{document/literal}.
"""

from suds import *
from suds.bindings.binding import Binding
from suds.sax.element import Element

from logging import getLogger

log = getLogger(__name__)


class Document(Binding):
    """
    The document/literal style. Literal is the only (@use) supported since
    document/encoded is pretty much dead.

    Although the SOAP specification supports multiple documents within the SOAP
    <body/>, it is very uncommon. As such, suds library supports presenting an
    I{RPC} view of service methods defined with only a single document
    parameter. To support the complete specification, service methods defined
    with multiple documents (multiple message parts), are still presented using
    a full I{document} view.

    More detailed description:
    
    An interface is considered I{wrapped} if:
      - There is exactly one message part in that interface.
      - The message part resolves to an element of a non-builtin type.
    Otherwise it is considered I{bare}.
    
    I{Bare} interface is Interpreted directly as specified in the WSDL schema,
    with each message part represented by a single parameter in the suds
    library web service operation proxy interface (input or output).
    
    I{Wrapped} interface is interpreted without the external wrapping document
    structure, with each of its contained elements passed through suds
    library's web service operation proxy interface (input or output)
    individually instead of as a single I{document} object.

    """
    def bodycontent(self, method, args, kwargs):
        if not len(method.soap.input.body.parts):
            return ()
        wrapped = method.soap.input.body.wrapped
        if wrapped:
            pts = self.bodypart_types(method)
            root = self.document(pts[0])
        else:
            root = []
        n = 0
        for pd in self.param_defs(method):
            if n < len(args):
                value = args[n]
            else:
                value = kwargs.get(pd[0])
            n += 1
            # Skip non-existing by-choice arguments.
            # Implementation notes:
            #   * This functionality might be better placed inside the mkparam()
            #     function but to do that we would first need to understand more
            #     thoroughly how different Binding subclasses in suds work and
            #     how they would be affected by this change.
            #   * If caller actually wishes to pass an empty choice parameter he
            #     can specify its value explicitly as an empty string.
            if len(pd) > 2 and pd[2] and value is None:
                continue
            p = self.mkparam(method, pd, value)
            if p is None:
                continue
            if not wrapped:
                ns = pd[1].namespace('ns0')
                p.setPrefix(ns[0], ns[1])
            root.append(p)
        return root

    def replycontent(self, method, body):
        wrapped = method.soap.output.body.wrapped
        if wrapped:
            return body[0].children
        return body.children

    def document(self, wrapper):
        """
        Get the document root. For I{document/literal}, this is the name of the
        wrapper element qualifed by the schema's target namespace.
        @param wrapper: The method name.
        @type wrapper: L{xsd.sxbase.SchemaObject}
        @return: A root element.
        @rtype: L{Element}
        """
        tag = wrapper[1].name
        ns = wrapper[1].namespace('ns0')
        d = Element(tag, ns=ns)
        return d

    def mkparam(self, method, pdef, object):
        """
        Expand list parameters into individual parameters each with the type
        information. This is because in document arrays are simply
        multi-occurrence elements.
        
        """
        if isinstance(object, (list, tuple)):
            tags = []
            for item in object:
                tags.append(self.mkparam(method, pdef, item))
            return tags
        return Binding.mkparam(self, method, pdef, object)

    def param_defs(self, method):
        """Get parameter definitions for document literal."""
        pts = self.bodypart_types(method)
        wrapped = method.soap.input.body.wrapped
        if not wrapped:
            return pts
        result = []
        # wrapped
        for p in pts:
            resolved = p[1].resolve()
            for child, ancestry in resolved:
                if child.isattr():
                    continue
                result.append((child.name, child, self.bychoice(ancestry)))
        return result

    def returned_types(self, method):
        result = []
        wrapped = method.soap.output.body.wrapped
        rts = self.bodypart_types(method, input=False)
        if wrapped:
            for pt in rts:
                resolved = pt.resolve(nobuiltin=True)
                for child, ancestry in resolved:
                    result.append(child)
                break
        else:
            result += rts
        return result

    def bychoice(self, ancestry):
        """
        The ancestry contains a <choice/>
        @param ancestry: A list of ancestors.
        @type ancestry: list
        @return: True if contains <choice/>
        @rtype: boolean
        """
        for x in ancestry:
            if x.choice():
                return True
        return False
