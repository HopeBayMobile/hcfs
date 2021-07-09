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
The plugin module provides classes for implementation
of suds plugins.
"""

from suds import *
from logging import getLogger

log = getLogger(__name__)


class Context(object):
    """
    Plugin context.
    """
    pass


class InitContext(Context):
    """
    Init Context.
    @ivar wsdl: The wsdl.
    @type wsdl: L{wsdl.Definitions}
    """
    pass


class DocumentContext(Context):
    """
    The XML document load context.
    @ivar url: The URL.
    @type url: str
    @ivar document: Either the XML text or the B{parsed} document root.
    @type document: (str|L{sax.element.Element})
    """
    pass


class MessageContext(Context):
    """
    The context for sending the SOAP envelope.
    @ivar envelope: The SOAP envelope to be sent.
    @type envelope: (str|L{sax.element.Element})
    @ivar reply: The reply.
    @type reply: (str|L{sax.element.Element}|object)
    """
    pass


class Plugin:
    """
    Plugin base.
    """
    pass


class InitPlugin(Plugin):
    """
    The base class for suds I{init} plugins.
    """

    def initialized(self, context):
        """
        Suds client initialization.
        Called after wsdl the has been loaded.  Provides the plugin
        with the opportunity to inspect/modify the WSDL.
        @param context: The init context.
        @type context: L{InitContext}
        """
        pass


class DocumentPlugin(Plugin):
    """
    The base class for suds I{document} plugins.
    """

    def loaded(self, context):
        """
        Suds has loaded a WSDL/XSD document.  Provides the plugin
        with an opportunity to inspect/modify the unparsed document.
        Called after each WSDL/XSD document is loaded.
        @param context: The document context.
        @type context: L{DocumentContext}
        """
        pass

    def parsed(self, context):
        """
        Suds has parsed a WSDL/XSD document.  Provides the plugin
        with an opportunity to inspect/modify the parsed document.
        Called after each WSDL/XSD document is parsed.
        @param context: The document context.
        @type context: L{DocumentContext}
        """
        pass


class MessagePlugin(Plugin):
    """
    The base class for suds I{SOAP message} plugins.
    """

    def marshalled(self, context):
        """
        Suds will send the specified soap envelope.
        Provides the plugin with the opportunity to inspect/modify
        the envelope Document before it is sent.
        @param context: The send context.
            The I{envelope} is the envelope document.
        @type context: L{MessageContext}
        """
        pass

    def sending(self, context):
        """
        Suds will send the specified SOAP envelope.
        Provides the plugin with the opportunity to inspect/modify
        the message text it is sent.
        @param context: The send context.
            The I{envelope} is the envelope text.
        @type context: L{MessageContext}
        """
        pass

    def received(self, context):
        """
        Suds has received the specified reply.
        Provides the plugin with the opportunity to inspect/modify
        the received XML text before it is SAX parsed.
        @param context: The reply context.
            The I{reply} is the raw text.
        @type context: L{MessageContext}
        """
        pass

    def parsed(self, context):
        """
        Suds has sax parsed the received reply.
        Provides the plugin with the opportunity to inspect/modify
        the sax parsed DOM tree for the reply before it is unmarshalled.
        @param context: The reply context.
            The I{reply} is DOM tree.
        @type context: L{MessageContext}
        """
        pass

    def unmarshalled(self, context):
        """
        Suds has unmarshalled the received reply.
        Provides the plugin with the opportunity to inspect/modify
        the unmarshalled reply object before it is returned.
        @param context: The reply context.
            The I{reply} is unmarshalled suds object.
        @type context: L{MessageContext}
        """
        pass


class PluginContainer:
    """
    Plugin container provides easy method invocation.
    @ivar plugins: A list of plugin objects.
    @type plugins: [L{Plugin},]
    @cvar ctxclass: A dict of plugin method / context classes.
    @type ctxclass: dict
    """

    domains = {\
        'init': (InitContext, InitPlugin),
        'document': (DocumentContext, DocumentPlugin),
        'message': (MessageContext, MessagePlugin ),
    }

    def __init__(self, plugins):
        """
        @param plugins: A list of plugin objects.
        @type plugins: [L{Plugin},]
        """
        self.plugins = plugins

    def __getattr__(self, name):
        domain = self.domains.get(name)
        if domain:
            plugins = []
            ctx, pclass = domain
            for p in self.plugins:
                if isinstance(p, pclass):
                    plugins.append(p)
            return PluginDomain(ctx, plugins)
        else:
            raise Exception, 'plugin domain (%s), invalid' % name


class PluginDomain:
    """
    The plugin domain.
    @ivar ctx: A context.
    @type ctx: L{Context}
    @ivar plugins: A list of plugins (targets).
    @type plugins: list
    """

    def __init__(self, ctx, plugins):
        self.ctx = ctx
        self.plugins = plugins

    def __getattr__(self, name):
        return Method(name, self)


class Method:
    """
    Plugin method.
    @ivar name: The method name.
    @type name: str
    @ivar domain: The plugin domain.
    @type domain: L{PluginDomain}
    """

    def __init__(self, name, domain):
        """
        @param name: The method name.
        @type name: str
        @param domain: A plugin domain.
        @type domain: L{PluginDomain}
        """
        self.name = name
        self.domain = domain

    def __call__(self, **kwargs):
        ctx = self.domain.ctx()
        ctx.__dict__.update(kwargs)
        for plugin in self.domain.plugins:
            try:
                method = getattr(plugin, self.name, None)
                if method and callable(method):
                    method(ctx)
            except Exception, pe:
                log.exception(pe)
        return ctx
