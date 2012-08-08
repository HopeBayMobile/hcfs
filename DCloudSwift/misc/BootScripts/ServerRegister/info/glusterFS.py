'''
Created on 2011/12/23

@author: jimmy
'''
#!/usr/bin/env python
#-*- coding: utf-8 -*-

class glusterFS_version(object):
    
    def get_version(self):
        
        gluster_version = open("glusterFS_version.txt").read()   
        return gluster_version