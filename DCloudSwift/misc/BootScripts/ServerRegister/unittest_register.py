#!/usr/bin/env python
#-*- coding: utf-8 -*-
'''
Created on 2011/11/16

@author: jimmy
'''
from info.disk import DiskInfo                       
from info.memory import MemoryInfo
from info.processor import ProcessorInfo
from info.NetworkEnvironment import NetworkEnvironment
from info.SwitchInfo import SwitchInfo
#from info.role import Server_role_Info 
from ConfigParser import SafeConfigParser
import register
import unittest
import os
    
register = register.Register()
register.get_system_processor_info()
register.get_system_storage_capacity()
register.get_system_memory_capacity()
register.get_system_network_info()
register.get_system_server_role_info()

#unittest_main()
class TestSequenceFunctions_Register(unittest.TestCase):
    def setUp(self):
        self.parser = SafeConfigParser()
        self.parser.read('register.conf')   
        
    def test__system_processor_info(self):                     #fetch /proc/cpuinfo內有關提供host info的值
        processor = ProcessorInfo()
        processor_info = processor.get_info()
        cpu_core_count = int(processor_info[0]['cpu cores'].strip())
        cpu_count = len(processor_info) / cpu_core_count
        
        cpu_flags = processor_info[0]['flags']
        
        for line in os.popen("cat /proc/cpuinfo | grep model| head -n 1 | awk '{printf $3}'"):
            cpu_model_name = str(line)
        for line in os.popen("cat /proc/cpuinfo | grep 'cpu MHz'| head -n 1 | awk '{printf $4}'"): 
            cpu_frequency_in_mhz = float(line)
        for line in os.popen("cat /proc/cpuinfo | grep 'model name'| head -n 1 | awk '{printf (\"%s %s %s %s %s %s\",\
                              $4,$5,$6,$7,$8,$9)}'"): #have to fetch 4-9 字元
            cpu_name = str(line)
        for line in os.popen("cat /proc/cpuinfo | grep 'cpu cores'| head -n 1 | awk '{printf $4}'"):
            cpu_core_count = str(line)
        for line in os.popen("cat /proc/cpuinfo | grep 'cpu family'| head -n 1 | awk '{printf $4}'"): 
            cpu_family = int(line)
#        for line in os.popen("cat /proc/cpuinfo | grep flags | head -n 1"):  #have to fetch 3-37 字元
#            first_cpu_flags = str(line)
#            cpu_flags = first_cpu_flags[9:]
#            print register.host_info["cpu_flags"]
#            print cpu_flags
        for line in os.popen("cat /proc/cpuinfo | grep vendor_id | head -n 1 | awk '{printf $3}'"): 
            cpu_vendor = str(line)
        self.assertEqual(register.host_info["cpu_count"], cpu_count, "not equal")
        self.assertEqual(register.host_info["cpu_model_name"], cpu_model_name, "not equal")
        self.assertEqual(float(register.host_info["cpu_frequency_in_mhz"]), cpu_frequency_in_mhz, "not equal")
        self.assertEqual(register.host_info["cpu_name"], cpu_name, "not equal")
        self.assertEqual(register.host_info["cpu_core_count"], cpu_core_count, "not eqsual")
        self.assertEqual(int(register.host_info["cpu_family"]), cpu_family, "not equal")
        self.assertEqual(register.host_info["cpu_flags"], cpu_flags, "not equal")
        self.assertEqual(register.host_info["cpu_vendor"], cpu_vendor, "not equal")
    
    def test__system_storage_capacity(self):                   #fetch harddisk capacity
        diskInfo = DiskInfo()
        disks = diskInfo.get_system_harddrives()              #disks = ['/dev/sda']
        capacity = 0
        print disks
        for disc in disks:
            harddisk_in_gb = capacity + diskInfo.get_capacity(disc) / 1000 / 1000 / 1000 # transfer GB
        self.assertEqual(register.host_info["harddisk_in_gb"], harddisk_in_gb, "not equal")
        
    def test__system_memory_capacity(self):                    #fetch memory capacity
        memory = MemoryInfo()
        mem_total = memory.get_capacity()
        memory_in_mb = mem_total / 1024
        self.assertEqual(register.host_info["memory_in_mb"], memory_in_mb, "not equal")
        
    def get_switch_info(self, switch_ipv4_address):            #fetch switch info
        switch = SwitchInfo(switch_ipv4_address)
        switch_mac_address = switch.get_hardware_address()
        return switch_mac_address
    
    def fetch_switch_port_number(self, switch_ipv4_address, mac_address):
        switch = SwitchInfo(switch_ipv4_address)
        switch_port_number = switch.fetch_port_number(mac_address)
        return switch_port_number
    
    def test_system_network_info(self):       
        host = NetworkEnvironment()
        nic_info = host.ifconfig                        #nic_info為有關網卡的所有資訊
        nics = []
        for nic in nic_info:                            #nic分為lo, virbr0, eth0, eth1
            ifname = nic
            if (ifname.find('eth') == 0 and nic_info[ifname]['ipv4'] != ""):
                data = {}
                data = nic_info[ifname]
                switch_addr = self.parser.get(ifname, 'switch_address')
                switch_mac_addr = self.get_switch_info(switch_addr)
                switch_port_number = self.fetch_switch_port_number(switch_addr, nic_info[ifname]['nic_mac'])
                nic_info[ifname]['switch_mac'] = switch_mac_addr
                nic_info[ifname]['switch_port_number'] = switch_port_number
                ifno = ifname.replace('eth', '')
                data['eth'] = int(ifno)
                #data[""]
                nics.append(data)
#                print nics
        hostname = os.popen("cat /etc/hostname")
        hostname = hostname.read().strip()
        base_image = os.popen("cat /etc/issue")
        baseimage = base_image.read().strip()
        self.assertEqual(register.host_info["nics"], nics, "not equal")
        self.assertEqual(register.host_info["hostname"], hostname, "not equal")
        self.assertEqual(register.host_info["base_image"], baseimage, "not equal")

    def test__system_server_role_info(self):                     #fetch server role
        role = open("server_role.txt").read()
        self.assertEqual(register.host_info["role"], role, "not equal")
        
if __name__ == '__main__':
    unittest.main()