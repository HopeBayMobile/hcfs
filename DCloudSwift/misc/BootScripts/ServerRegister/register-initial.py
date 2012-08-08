#!/usr/bin/env python
#-*- coding: utf-8 -*-

from info.disk import DiskInfo                                #從info內的disk.py擷取DiskInfo的function
from info.memory import MemoryInfo 
from info.processor import ProcessorInfo
from info.NetworkEnvironment import NetworkEnvironment
from info.SwitchInfo import SwitchInfo
from info.role import Server_role_Info 
from info.glusterFS import glusterFS_version
import urllib,httplib                                         #使用urllib擷取網頁的原始碼
import json
from ConfigParser import SafeConfigParser                     #將一些設定獨立出成一個設定檔
__DEBUG__ = False
__DEBUG__ = True
'''print "Server_role is 'default' or 'dynamic'"
reply = raw_input()'''
    
class Register(object):
    host_info = {}                                            #設定default value
    config = None
    def __init__(self):
        '''
        Constructor
        '''
        self.__class__.config = SafeConfigParser()
        self.__class__.config.read('register.conf')           #讀register.config檔
    # get hard drive capacity
    def get_system_storage_capacity(self):
        diskInfo = DiskInfo()
        disks = diskInfo.get_system_harddrives()              #disks = ['/dev/sda']
        capacity = 0
        for disc in disks:
            capacity = capacity + diskInfo.get_capacity(disc) / 1000 / 1000 / 1000 # transfer GB
        self.__class__.host_info["harddisk_in_gb"] = capacity
    
    def get_system_memory_capacity(self):
        memory = MemoryInfo()
        mem_total = memory.get_capacity()
        mem_total = mem_total / 1024
        self.__class__.host_info["memory_in_mb"] = mem_total
    
    def get_system_processor_info(self):
        processor = ProcessorInfo()
        processor_info = processor.get_info()
        cpu_core_count = int(processor_info[0]['cpu cores'].strip()) #processor_info[0]為選定第一段的info
        try:
            cpu_count = len(processor_info) / cpu_core_count      #processor_info代表host端有四個thread
        except:
            cpu_count = 1
        self.__class__.host_info["cpu_vendor"] = processor_info[0]['vendor_id'].strip()  #strip用來去除˙空白
        self.__class__.host_info["cpu_family"] = processor_info[0]['cpu family'].strip()
        self.__class__.host_info["cpu_model_name"] = processor_info[0]['model'].strip()
        self.__class__.host_info["cpu_core_count"] = processor_info[0]['cpu cores'].strip()
        self.__class__.host_info["cpu_frequency_in_mhz"] = processor_info[0]['cpu MHz'].strip()
        self.__class__.host_info["cpu_name"] = processor_info[0]['model name'].strip()
        self.__class__.host_info["cpu_count"] = cpu_count
        self.__class__.host_info["cpu_flags"] = processor_info[0]['flags']
    
    def get_system_network_info(self):
        host = NetworkEnvironment()
        nic_info = host.ifconfig                        #nic_info為有關網卡的所有資訊
        nics = []
        for nic in nic_info:                            #nic分為lo, virbr0, eth0, eth1
            ifname = nic
            if (ifname.find('eth') == 0 and nic_info[ifname]['ipv4'] != ""):
                data = {}
                data = nic_info[ifname]
                switch_addr = self.__class__.config.get(ifname, 'switch_address')
                switch_mac_addr = self.get_switch_info(switch_addr)
                switch_port_number = self.fetch_switch_port_number(switch_addr, nic_info[ifname]['nic_mac'])
                nic_info[ifname]['switch_mac'] = switch_mac_addr
                nic_info[ifname]['switch_port_number'] = switch_port_number
                ifno = ifname.replace('eth', '')
                data['eth'] = int(ifno)
                #data[""]
                nics.append(data)
        self.__class__.host_info['nics'] = nics
        self.__class__.host_info['hostname'] = host.get_hostname()
        self.__class__.host_info['base_image'] = host.get_baseimage()
        
    def get_system_server_role_info(self):
        role = Server_role_Info()
        '''while reply:                                                 #判斷role要動態取得或預設
            if reply == 'dynamic':
                self.__class__.host_info['role'] = role.get_role_fun1()
                break
            else:
                self.__class__.host_info['role'] = role.get_role_fun2()
                break'''
        self.__class__.host_info['role'] = role.get_role_fun2()
    
    def get_system_volume_version(self):
        gFS_version = glusterFS_version()
        self.__class__.host_info['gFS_version'] = gFS_version.get_version()
        
        
    def get_switch_info(self, switch_ipv4_address):
        switch = SwitchInfo(switch_ipv4_address)
        switch_mac_address = switch.get_hardware_address()
        return switch_mac_address
    
    def fetch_switch_port_number(self, switch_ipv4_address, mac_address):
        switch = SwitchInfo(switch_ipv4_address)
        switch_port_number = switch.fetch_port_number(mac_address)
        return switch_port_number
    
    def data_send(self):
        '''
        data_send()
        send host info to managerment server.
        '''
        serverAddr = self.__class__.config.get('managerserver', 'host_addr')
        serverPort = self.__class__.config.getint('managerserver', 'host_port')
        records = {}
        records['host_info'] = json.dumps(self.__class__.host_info)
        if __DEBUG__:
            print records
        jsondata = urllib.urlencode(records)
        if __DEBUG__:
            print jsondata
        #print jsondata
        url =  "%s:%d" % (serverAddr, serverPort)
        #
        print 'Host information sending....'
        conn = httplib.HTTPConnection(url)
        try:
            conn.request("POST", "/hosts/register/", jsondata, headers = {'content-type': 'text/html'})
            response = conn.getresponse()
            result = response.read()
            if __DEBUG__:
                print response
                print response.status, response.reason
                print result
        except:
            print "Server is down!"

    def fetch_info(self):
        '''
        fetch_info()
        detect host all information.
        '''
        self.get_system_processor_info()
        self.get_system_storage_capacity()
        self.get_system_memory_capacity()
        self.get_system_network_info()
        self.get_system_server_role_info()
        self.get_system_volume_version()

    def look(self):
        '''
        look()
        
        display all information from host.
        '''
        print self.__class__.host_info
        
'''
main()
'''
if __name__ == '__main__':
    register = Register()
    register.fetch_info()
    if __DEBUG__:        
        register.look()
    else:
        register.data_send()   
        register.look()
