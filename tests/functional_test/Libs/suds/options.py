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
Suds basic options classes.
"""

from suds.cache import Cache, NoCache
from suds.properties import *
from suds.store import DocumentStore, defaultDocumentStore
from suds.transport import Transport
from suds.wsse import Security
from suds.xsd.doctor import Doctor


class TpLinker(AutoLinker):
    """
    Transport (auto) linker used to manage linkage between
    transport objects Properties and those Properties that contain them.
    """

    def updated(self, properties, prev, next):
        if isinstance(prev, Transport):
            tp = Unskin(prev.options)
            properties.unlink(tp)
        if isinstance(next, Transport):
            tp = Unskin(next.options)
            properties.link(tp)


class Options(Skin):
    """
    Options:
        - B{cache} - The XML document cache. May be set to None for no caching.
                - type: L{Cache}
                - default: L{NoCache()}
        - B{documentStore} - The XML document store used to access locally
            stored documents without having to download them from an external
            location. May be set to None for no internal suds library document
            store.
                - type: L{DocumentStore}
                - default: L{defaultDocumentStore}
        - B{faults} - Raise faults raised by server, else return tuple from
            service method invocation as (httpcode, object).
                - type: I{bool}
                - default: True
        - B{service} - The default service name.
                - type: I{str}
                - default: None
        - B{port} - The default service port name, not tcp port.
                - type: I{str}
                - default: None
        - B{location} - This overrides the service port address I{URL} defined
            in the WSDL.
                - type: I{str}
                - default: None
        - B{transport} - The message transport.
                - type: L{Transport}
                - default: None
        - B{soapheaders} - The soap headers to be included in the soap message.
                - type: I{any}
                - default: None
        - B{wsse} - The web services I{security} provider object.
                - type: L{Security}
                - default: None
        - B{doctor} - A schema I{doctor} object.
                - type: L{Doctor}
                - default: None
        - B{xstq} - The B{x}ml B{s}chema B{t}ype B{q}ualified flag indicates
            that the I{xsi:type} attribute values should be qualified by
            namespace.
                - type: I{bool}
                - default: True
        - B{prefixes} - Elements of the soap message should be qualified (when
            needed) using XML prefixes as opposed to xmlns="" syntax.
                - type: I{bool}
                - default: True
        - B{retxml} - Flag that causes the I{raw} soap envelope to be returned
            instead of the python object graph.
                - type: I{bool}
                - default: False
        - B{prettyxml} - Flag that causes I{pretty} xml to be rendered when
            generating the outbound soap envelope.
                - type: I{bool}
                - default: False
        - B{autoblend} - Flag that ensures that the schema(s) defined within
            the WSDL import each other.
                - type: I{bool}
                - default: False
        - B{cachingpolicy} - The caching policy.
                - type: I{int}
                  - 0 = Cache XML documents.
                  - 1 = Cache WSDL (pickled) object.
                - default: 0
        - B{plugins} - A plugin container.
                - type: I{list}
                - default: I{list()}
        - B{nosend} - Create the soap envelope but do not send.
            When specified, method invocation returns a I{RequestContext}
            instead of sending it.
                - type: I{bool}
                - default: False
    """
    def __init__(self, **kwargs):
        domain = __name__
        definitions = [
            Definition('cache', Cache, NoCache()),
            Definition('documentStore', DocumentStore, defaultDocumentStore),
            Definition('faults', bool, True),
            Definition('transport', Transport, None, TpLinker()),
            Definition('service', (int, basestring), None),
            Definition('port', (int, basestring), None),
            Definition('location', basestring, None),
            Definition('soapheaders', (), ()),
            Definition('wsse', Security, None),
            Definition('doctor', Doctor, None),
            Definition('xstq', bool, True),
            Definition('prefixes', bool, True),
            Definition('retxml', bool, False),
            Definition('prettyxml', bool, False),
            Definition('autoblend', bool, False),
            Definition('cachingpolicy', int, 0),
            Definition('plugins', (list, tuple), []),
            Definition('nosend', bool, False),
            Definition('unwrap', bool, True)]
        Skin.__init__(self, domain, definitions, kwargs)
