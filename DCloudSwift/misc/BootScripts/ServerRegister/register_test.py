#!/usr/bin/env python
#-*- coding: utf-8 -*-

#from info.disk import DiskInfo                              
#from info.memory import MemoryInfo 
#from info.processor import ProcessorInfo
#from info.NetworkEnvironment import NetworkEnvironment
#from info.SwitchInfo import SwitchInfo
#from info.role import Server_role_Info 
#from info.glusterFS import glusterFS_version
import urllib,httplib                                         
import json
import random
from ConfigParser import SafeConfigParser                     

__DEBUG__ = False
'''print "Server_role is 'default' or 'dynamic'"
reply = raw_input()'''

class Register(object):
    host_info = {}                                           
    config = None
    def __init__(self):
        '''
        Constructor
        '''
        #print "hostname:" + "TPE1AA0" + "202"
        #ifconfig={}
        #ifaces=[]
        nic_mac=":".join([str(hex(random.randint(10,255)))[2:] for x in range(6)])
        #rand=random.randrange(0,180,1)
        #rand=random.choice(choice)
        #nic_mac= nic_mac + str(rand)
        #print rand
         
        self.__class__.config = SafeConfigParser()
        self.__class__.config.read('register.conf')   
        self.__class__.host_info["harddisk_in_gb"] = 800    
        self.__class__.host_info["memory_in_mb"] = 16083	
        self.__class__.host_info["cpu_vendor"] = "AuthenticAMD"  
        self.__class__.host_info["cpu_family"] = 15
        self.__class__.host_info["cpu_model_name"] = 333
        self.__class__.host_info["cpu_core_count"] = 2
        self.__class__.host_info["cpu_frequency_in_mhz"] = 1000.000
        self.__class__.host_info["cpu_name"] = 2
        self.__class__.host_info["cpu_count"] = 2
        self.__class__.host_info["cpu_flags"] = "fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow rep_good nopl pni lahf_lm cmp_legacy"
        self.__class__.host_info['nics'] = [{'nic_mac':nic_mac, 'switch_port_number': -1, 'switch_mac': '12:55:f7:34:gh:46', 'ipv4': '172.16.229.102', 'ipv6': 'fe80::214:85ff:fef0:9cc2', 'name': "br100"}]
        #self.__class__.host_info['hostname'] = "TPE1AA0103"
        self.__class__.host_info['base_image'] = "Ubuntu 11.04"
        self.__class__.host_info['role'] = "system"
        self.__class__.host_info['gFS_version'] = '0.1'

        # get hard drive capacity
 
    '''def get_system_storage_capacity(self):
        diskInfo = DiskInfo()
        disks = diskInfo.get_system_harddrives()             
        #print disks
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
        cpu_core_count = int(processor_info[0]['cpu cores'].strip()) 
        try:
            cpu_count = len(processor_info) / cpu_core_count      
        except:
            cpu_count = 1
        self.__class__.host_info["cpu_vendor"] = processor_info[0]['vendor_id'].strip()  
        self.__class__.host_info["cpu_family"] = processor_info[0]['cpu family'].strip()
        self.__class__.host_info["cpu_model_name"] = processor_info[0]['model'].strip()
        self.__class__.host_info["cpu_core_count"] = processor_info[0]['cpu cores'].strip()
        self.__class__.host_info["cpu_frequency_in_mhz"] = processor_info[0]['cpu MHz'].strip()
        self.__class__.host_info["cpu_name"] = processor_info[0]['model name'].strip()
        self.__class__.host_info["cpu_count"] = cpu_count
        self.__class__.host_info["cpu_flags"] = processor_info[0]['flags']
    
    def get_system_network_info(self):
        host = NetworkEnvironment()
        nic_info = host.ifconfig                        
        #print nic_info['br100']
	nics = []
        for nic in nic_info:                          
            ifname = nic
	    #print ifname
             
	    if (ifname == 'br100' and nic_info[ifname]['ipv4'] != ""):
                data = {}
                data = nic_info[ifname]
                switch_addr = self.__class__.config.get(ifname, 'switch_address')
                switch_mac_addr = self.get_switch_info(switch_addr)
                switch_port_number = self.fetch_switch_port_number(switch_addr, nic_info[ifname]['nic_mac'])
                nic_info[ifname]['switch_mac'] = switch_mac_addr
                nic_info[ifname]['switch_port_number'] = switch_port_number
                #print ifname
		#ifno = ifname.replace('br', '')
		ifno = ifname
		data['name'] = ifno
                #data[""]
                nics.append(data)
        self.__class__.host_info['nics'] = nics
	#self.__class__.host_info['nics'] = [{'nic_mac': '', 'switch_port_number': -1, 'switch_mac': '', 'ipv4': '172.16.229.102', 'ipv6': 'fe80::214:85ff:fef0:9cc2', 'eth': 0}]
        self.__class__.host_info['hostname'] = host.get_hostname()
        self.__class__.host_info['base_image'] = host.get_baseimage()
        
    def get_system_server_role_info(self):
        role = Server_role_Info()
                #print response
        
	
	while reply:                                               
            if reply == 'dynamic':
                self.__class__.host_info['role'] = role.get_role_fun1()
                break
            else:
                self.__class__.host_info['role'] = role.get_role_fun2()
                break
	
        
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
	'''
    
    def data_send(self):
        '''
        data_send()
        send host info to managerment server.
        '''
        global register_info
        
        #global count    
        #count = 1
        print "============"
        hostname = "TPE1AA01" + "%02d" % count
        print hostname
        print "============"
        '''
        count = count + 1
        '''
        serverAddr = self.__class__.config.get('managerserver', 'host_addr')
        serverPort = self.__class__.config.getint('managerserver', 'host_port')
        self.__class__.host_info['hostname'] = hostname
        #self.__class__.host_info['hostname'] = "TPE1AA0101"
        records = {}
        records['host_info'] = json.dumps(self.__class__.host_info)
        if __DEBUG__:
            print records
        jsondata = urllib.urlencode(records)
        '''jsondata1 = os.popen("cat jsondata.txt")
	    print jsondata1'''
        if __DEBUG__:
            print jsondata
        #print jsondata
        url =  "%s:%d" % (serverAddr, serverPort)
        #
        print 'Host information sending....'
        conn = httplib.HTTPConnection(url)
        try:
            conn.request("POST", "/host/register/", jsondata, headers = {'content-type': 'text/html'})
            response = conn.getresponse()
            result = response.read()
            register_info = int(result)
            if __DEBUG__:
                print response
                print response.status, response.reason
                print result
            else:
                print result
                print response.status, response.reason
        except:
            print "Server is down!"
    '''
    def fetch_info(self):
        
        fetch_info()
        detect host all information.
        
        self.get_system_processor_info()
        self.get_system_storage_capacity()
        self.get_system_memory_capacity()
        self.get_system_network_info()
        self.get_system_server_role_info()
        self.get_system_volume_version()
    '''
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
        a=0
        #register.fetch_info()
        #global register_info
        if __DEBUG__:        
            register.look()
        else:
            count = 1	
            register.data_send()
            #global count 
            #print count
            '''
	        while(a<10):
			if(register_info != 0):
				register.data_send()
			count=count+1
			a=a+1
		    '''
            for x in range(10):
                register.data_send()
                count=count+1

                register.look()
