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
Provides modules containing classes to support
unmarshalling (XML).
"""

from suds.sudsobject import Object



class Content(Object):
    """
    @ivar node: The content source node.
    @type node: L{sax.element.Element}
    @ivar data: The (optional) content data.
    @type data: L{Object}
    @ivar text: The (optional) content (xml) text.
    @type text: basestring
    """

    extensions = []

    def __init__(self, node, **kwargs):
        Object.__init__(self)
        self.node = node
        self.data = None
        self.text = None
        for k,v in kwargs.items():
            setattr(self, k, v)

    def __getattr__(self, name):
        if name not in self.__dict__:
            if name in self.extensions:
                v = None
                setattr(self, name, v)
            else:
                raise AttributeError, \
                    'Content has no attribute %s' % name
        else:
            v = self.__dict__[name]
        return v
