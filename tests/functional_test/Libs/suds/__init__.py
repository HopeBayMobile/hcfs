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
Suds is a lightweight SOAP Python client providing a Web Service proxy.
"""

import sys


#
# Project properties
#

from version import __build__, __version__


#
# Exceptions
#

class MethodNotFound(Exception):
    def __init__(self, name):
        Exception.__init__(self, u"Method not found: '%s'" % name)

class PortNotFound(Exception):
    def __init__(self, name):
        Exception.__init__(self, u"Port not found: '%s'" % name)

class ServiceNotFound(Exception):
    def __init__(self, name):
        Exception.__init__(self, u"Service not found: '%s'" % name)

class TypeNotFound(Exception):
    def __init__(self, name):
        Exception.__init__(self, u"Type not found: '%s'" % tostr(name))

class BuildError(Exception):
    msg = """
        An error occured while building an instance of (%s). As a result the
        object you requested could not be constructed. It is recommended that
        you construct the type manually using a Suds object. Please open a
        ticket with a description of this error.
        Reason: %s
        """
    def __init__(self, name, exception):
        Exception.__init__(self, BuildError.msg % (name, exception))

class SoapHeadersNotPermitted(Exception):
    msg = """
        Method (%s) was invoked with SOAP headers. The WSDL does not define
        SOAP headers for this method. Retry without the soapheaders keyword
        argument.
        """
    def __init__(self, name):
        Exception.__init__(self, self.msg % name)

class WebFault(Exception):
    def __init__(self, fault, document):
        if hasattr(fault, 'faultstring'):
            Exception.__init__(self, u"Server raised fault: '%s'" %
                fault.faultstring)
        self.fault = fault
        self.document = document


#
# Logging
#

class Repr:
    def __init__(self, x):
        self.x = x
    def __str__(self):
        return repr(self.x)


#
# Utility
#

class null:
    """
    The I{null} object.
    Used to pass NULL for optional XML nodes.
    """
    pass

def objid(obj):
    return obj.__class__.__name__ + ':' + hex(id(obj))

def tostr(object, encoding=None):
    """ get a unicode safe string representation of an object """
    if isinstance(object, basestring):
        if encoding is None:
            return object
        else:
            return object.encode(encoding)
    if isinstance(object, tuple):
        s = ['(']
        for item in object:
            if isinstance(item, basestring):
                s.append(item)
            else:
                s.append(tostr(item))
            s.append(', ')
        s.append(')')
        return ''.join(s)
    if isinstance(object, list):
        s = ['[']
        for item in object:
            if isinstance(item, basestring):
                s.append(item)
            else:
                s.append(tostr(item))
            s.append(', ')
        s.append(']')
        return ''.join(s)
    if isinstance(object, dict):
        s = ['{']
        for item in object.items():
            if isinstance(item[0], basestring):
                s.append(item[0])
            else:
                s.append(tostr(item[0]))
            s.append(' = ')
            if isinstance(item[1], basestring):
                s.append(item[1])
            else:
                s.append(tostr(item[1]))
            s.append(', ')
        s.append('}')
        return ''.join(s)
    try:
        return unicode(object)
    except:
        return str(object)


#
# Python 3 compatibility
#

if sys.version_info < (3, 0):
    from cStringIO import StringIO as BytesIO
else:
    from io import BytesIO

# Idea from 'http://lucumr.pocoo.org/2011/1/22/forwards-compatible-python'.
class UnicodeMixin(object):
    if sys.version_info >= (3, 0):
        # For Python 3, __str__() and __unicode__() should be identical.
        __str__ = lambda x: x.__unicode__()
    else:
        __str__ = lambda x: unicode(x).encode('utf-8')

#   Used instead of byte literals because they are not supported on Python
# versions prior to 2.6.
def byte_str(s='', encoding='utf-8', input_encoding='utf-8', errors='strict'):
    """
    Returns a bytestring version of 's', encoded as specified in 'encoding'.

    Accepts str & unicode objects, interpreting non-unicode strings as byte
    strings encoded using the given input encoding.

    """
    assert isinstance(s, basestring)
    if isinstance(s, unicode):
        return s.encode(encoding, errors)
    if s and encoding != input_encoding:
        return s.decode(input_encoding, errors).encode(encoding, errors)
    return s

# Class used to represent a byte string. Useful for asserting that correct
# string types are being passed around where needed.
if sys.version_info >= (3, 0):
    byte_str_class = bytes
else:
    byte_str_class = str
