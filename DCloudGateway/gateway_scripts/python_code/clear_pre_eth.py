'''
check if there are previous NIC on the system,
if yes, remove them, and make sure only two nics exist



@author: JASHING.HUANG
'''

import subprocess
import re

max_nic_number = 1  # eth0, eth1
# if ethx, where x >=2, them need to clear "/etc/udev/rules.d/70-persistent-net.rules" 


nicfn = "/etc/udev/rules.d/70-persistent-net.rules"

def Clear_NICs_Reboot():
    cmd = "echo > " + nicfn
    reboot_cmd = "reboot now"
    
    try:
        # clear the file
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
        
        # reboot now
        po = subprocess.Popen(reboot_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
        
        
    except:
        # add md fail,  
        return False
    
    return True
    
# return number of working MD disk
def Num_NICs():
    
    nics = re.compile(".+ NAME=\"eth(?P<nic_name>\d+)\"")

    max_nic = 0

    # Open a file
    fo = open(nicfn, "r+")
    while True:
        line = fo.readline()
        if not line:
            break
                
        m = nics.match(line)
            
        if m == None:
            continue 
            
        nic = int(m.group("nic_name"))
        #print nic
        
        if nic > max_nic:
            max_nic = nic
            
            
    fo.close()
    
    return max_nic
####################################
# main

max_nic_num = Num_NICs()

if max_nic_num >= 2:
    Clear_NICs_Reboot()
    
