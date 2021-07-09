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
Provides classes for the (WS) SOAP I{rpc/literal} and I{rpc/encoded} bindings.
"""

from logging import getLogger
from suds import *
from suds.mx.encoded import Encoded as MxEncoded
from suds.umx.encoded import Encoded as UmxEncoded
from suds.bindings.binding import Binding, envns
from suds.sax.element import Element

log = getLogger(__name__)


encns = ('SOAP-ENC', 'http://schemas.xmlsoap.org/soap/encoding/')

class RPC(Binding):
    """
    RPC/Literal binding style.
    """

    def param_defs(self, method):
        return self.bodypart_types(method)

    def envelope(self, header, body):
        env = Binding.envelope(self, header, body)
        env.addPrefix(encns[0], encns[1])
        env.set('%s:encodingStyle' % envns[0],
                'http://schemas.xmlsoap.org/soap/encoding/')
        return env

    def bodycontent(self, method, args, kwargs):
        n = 0
        root = self.method(method)
        for pd in self.param_defs(method):
            if n < len(args):
                value = args[n]
            else:
                value = kwargs.get(pd[0])
            p = self.mkparam(method, pd, value)
            if p is not None:
                root.append(p)
            n += 1
        return root

    def replycontent(self, method, body):
        return body[0].children

    def method(self, method):
        """
        Get the document root.  For I{rpc/(literal|encoded)}, this is the
        name of the method qualifed by the schema tns.
        @param method: A service method.
        @type method: I{service.Method}
        @return: A root element.
        @rtype: L{Element}
        """
        ns = method.soap.input.body.namespace
        if ns[0] is None:
            ns = ('ns0', ns[1])
        method = Element(method.name, ns=ns)
        return method


class Encoded(RPC):
    """
    RPC/Encoded (section 5)  binding style.
    """

    def marshaller(self):
        return MxEncoded(self.schema())

    def unmarshaller(self):
        """
        Get the appropriate schema based XML decoder.
        @return: Typed unmarshaller.
        @rtype: L{UmxTyped}
        """
        return UmxEncoded(self.schema())
