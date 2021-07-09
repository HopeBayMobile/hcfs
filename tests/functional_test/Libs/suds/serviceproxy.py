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
The service proxy provides access to web services.

Replaced by: L{client.Client}
"""

from logging import getLogger
from suds import *
from suds.client import Client

log = getLogger(__name__)


class ServiceProxy(UnicodeMixin):

    """
    A lightweight soap based web service proxy.
    @ivar __client__: A client.
        Everything is delegated to the 2nd generation API.
    @type __client__: L{Client}
    @note:  Deprecated, replaced by L{Client}.
    """

    def __init__(self, url, **kwargs):
        """
        @param url: The URL for the WSDL.
        @type url: str
        @param kwargs: keyword arguments.
        @keyword faults: Raise faults raised by server (default:True),
                else return tuple from service method invocation as (http code, object).
        @type faults: boolean
        @keyword proxy: An http proxy to be specified on requests (default:{}).
                           The proxy is defined as {protocol:proxy,}
        @type proxy: dict
        """
        client = Client(url, **kwargs)
        self.__client__ = client

    def get_instance(self, name):
        """
        Get an instance of a WSDL type by name
        @param name: The name of a type defined in the WSDL.
        @type name: str
        @return: An instance on success, else None
        @rtype: L{sudsobject.Object}
        """
        return self.__client__.factory.create(name)

    def get_enum(self, name):
        """
        Get an instance of an enumeration defined in the WSDL by name.
        @param name: The name of a enumeration defined in the WSDL.
        @type name: str
        @return: An instance on success, else None
        @rtype: L{sudsobject.Object}
        """
        return self.__client__.factory.create(name)

    def __unicode__(self):
        return unicode(self.__client__)

    def __getattr__(self, name):
        builtin = name.startswith('__') and name.endswith('__')
        if builtin:
            return self.__dict__[name]
        else:
            return getattr(self.__client__.service, name)
