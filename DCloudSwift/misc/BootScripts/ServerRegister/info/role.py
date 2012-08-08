'''
Created on 2011/11/9

@author: jimmy
'''
#!/usr/bin/env python
#-*- coding: utf-8 -*-
#import random

class Server_role_Info(object):
    
    '''def get_role_fun1(self):
        
        role = {'system zone':'system','default zone':['compute','storage']}
        Real_Role = {}
        x = random.randrange(2)
        
        if x == 0:
            Real_Role = role['system zone']
        else:
            y = random.randrange(2)
            if y ==0:
                Real_Role = role['default zone'][y]
            else:
                Real_Role = role['default zone'][y]
        return Real_Role'''
    
    def get_role_fun2(self):
        Real_Role = open("server_role.txt").read()
        return Real_Role