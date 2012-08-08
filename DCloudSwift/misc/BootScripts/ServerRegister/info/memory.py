#!/usr/bin/env python
#-*- coding: utf-8 -*-
'''
Created on 2011/6/15

@author: Juitsung
'''
__version__ = "$Id"

import os

class MemoryInfo(object):
    '''
    classdocs
    '''
    info = None

    def __init__(self):
        '''
        Constructor
        '''
        if self.info is not None:
            return

    def get_capacity(self):
        capacity = 0      #popen的用法很類似open,但他是接command line的語法
        for line in os.popen("cat /proc/meminfo|grep MemTotal|sed \"s/[\.:=]/ /g\"|awk '{printf(\"%d\", $2)}'"):
#equal cat /proc/meminfo|awk '/MemTotal/{printf $2}'
            capacity = int(line)
        return capacity