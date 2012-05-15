#!/usr/bin/env python
#-*- coding: utf-8 -*-
'''
Created on 2011/6/15

@author: cloudusr
'''
__version__ = "$Id"

import string

class ProcessorInfo:

    info = None

    def __init__(self):                #先在自己的class內執行
        if self.info is not None:
            return
        info = []
        try:
            for line in open('/proc/cpuinfo').readlines():
                fieldvalue = map(string.strip, string.split(line,':',1))
                if len(fieldvalue) != 2:
                    continue
                field,value = fieldvalue     #這邊的fieldvalue還不是dict的格式
                if not info or info[-1].has_key(field): # if (!(info) || index_exists(field)), 這邊的-1代表index
                    info.append({})
                info[-1][field] = value  #印出/proc/cpuinfo所有value值
        except:
            print 'read hard drive info fail!'
        self.__class__.info = info
        
    def get_info(self):
        return self.__class__.info 
    
#if __name__ == '__main__':
#    info = ProcessorInfo().get_info()
#    print info
