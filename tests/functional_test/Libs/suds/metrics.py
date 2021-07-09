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
The I{metrics} module defines classes and other resources
designed for collecting and reporting performance metrics.
"""

import time
from logging import getLogger
from suds import *
from math import modf

log = getLogger(__name__)

class Timer:

    def __init__(self):
        self.started = 0
        self.stopped = 0

    def start(self):
        self.started = time.time()
        self.stopped = 0
        return self

    def stop(self):
        if self.started > 0:
            self.stopped = time.time()
        return self

    def duration(self):
        return ( self.stopped - self.started )

    def __str__(self):
        if self.started == 0:
            return 'not-running'
        if self.started > 0 and self.stopped == 0:
            return 'started: %d (running)' % self.started
        duration = self.duration()
        jmod = ( lambda m : (m[1], m[0]*1000) )
        if duration < 1:
            ms = (duration*1000)
            return '%d (ms)' % ms
        if duration < 60:
            m = modf(duration)
            return '%d.%.3d (seconds)' % jmod(m)
        m = modf(duration/60)
        return '%d.%.3d (minutes)' % jmod(m)
