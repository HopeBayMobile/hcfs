'''
for adding a new HDD and rebuilding RAID1.

the new HDD must be empty, i.e., no partition on it.

Created on 2012/8/31

@author: JASHING.HUANG
'''

import subprocess
import re

# return number of working MD disk, if <2, return 0
def ChkMDDrive():
    workdev = re.compile("^Working Devices :\s+(?P<devnum>.+)")
    cmd = "mdadm -D /dev/md127"

    try:
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return 0
        
            
        # get num of working drive
        lines = output.split("\n")
        
        for line in lines:
            #print line
            
            m = workdev.match(line)
            
            if m == None:
                continue 

            dn = int(m.group("devnum"))
            #print dn
            
            if dn >=2:
                #print "dn>=2"
                return dn
        
    except:
        # add md fail,  
        #print "????"
        return 0
    
    return 0
    

def AddMDDisk(newPartedDisk, debug=False):
    cmd = " mdadm /dev/md127 --add /dev/" + newPartedDisk
    if debug == True:
        print cmd
        return True
     
    try:
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
    except:
        # add md fail,  
        return False
    
    return True


def PartNewHDD(disk, debug=False):
    cmd1 = "parted -s /dev/" + disk + " mklabel msdos"
    cmd2 = "parted -s /dev/" + disk + " mkpart primary 0 100%"
    cmd3 = "parted -s /dev/" + disk + " set 1 raid on"

    if debug == True:
        print cmd1
        print cmd2
        print cmd3
        return True
    
    try:
        po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
        
        po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
    
        po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            print "error" + output
            return False
    except:
        # partition fail,  
        return False
    
    return True

# only size >= 200GB will be accepted
def Filter_NewHDD():
    
    procfile = open("/proc/partitions")
    parts = [p.split() for p in procfile.readlines()[2:]]
    procfile.close()
    
    DEVs = []
    for p in parts:
        devname = p[3]
        devsize = p[2]

        if devname.startswith("md"):
            continue

        # only care about sda, sdb, sdx...
        if len(devname) == 3:
            if devsize >= 200 * 1024 * 1024 * 1024:
                DEVs.append(devname)
        
    return DEVs

def Parse_NewHDD():
    
    AvaiDevs = Filter_NewHDD()
    
    #fdiskre = re.compile(".*/dev/(?P<diskname>[^:\s+]+)GB")
    fdiskre = re.compile(".*/dev/(?P<diskname>[^:\s+]+)")
    cmd = "fdisk -l"
     
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()
    if po.returncode != 0:
        print "error" + output
    
    lines = output.split("\n")
    
    # case of "sdc" no part
    #lines = fdisk_info.split("\n") 
    
    diskname = None
    diskpart = None

    diskarr = []
    
    # find all disk and part.
    for line in lines:
        #print line
        m = fdiskre.match(line)
        if m == None:
            pass 
        else:
            dn = m.group('diskname')
            #print dn
            if dn.startswith("md"):# or dn.startswith("sda"):
                continue
            
            for ad in AvaiDevs:
                if dn.startswith(ad):
                    diskarr.append(dn)
            
    print diskarr
    
    # find no parted disk
    for dn in diskarr:
        if len(dn) == 3: # sda
            parted_flag = False
            dnp = dn + "1" # sda1
            
            for tmp in diskarr:
                if tmp == dnp:
                    # this disk has partition
                    parted_flag = True
                
            if parted_flag == False: # no parted
                return dn 
                
    return None

# add a new disk (must be no any partition on it) to MD
def Add_Disk_and_Rebuild_MD():
    
    num_working_raid_dev = ChkMDDrive()
    
    if num_working_raid_dev >=2: # raid 1 is fine
        return num_working_raid_dev
     
    # find disk w/o partition
    noPartedDisk = Parse_NewHDD()
    #print noPartedDisk
    #return 
    
    # no such a disk
    if noPartedDisk == None:
        #do nothing
        return ChkMDDrive() # return num of working devs 
    
    #noPartedDisk = "sdd"
    #partition the disk
    partdisk = PartNewHDD(noPartedDisk, False)
    #partdisk = PartNewHDD(noPartedDisk, True)
    #print partdisk
    
    #newPartedDisk = "sdd" + "1"
    
    newPartedDisk = noPartedDisk + "1"

    # add the new parted disk to MD    
    addMDdisk = AddMDDisk(newPartedDisk, False)
    #addMDdisk = AddMDDisk(newPartedDisk, True)
    print addMDdisk
    
    # chk if success
    rb_ok = ChkMDDrive()
    #print rb_ok
    
    return rb_ok

####################################
# main
status = Add_Disk_and_Rebuild_MD()
print status

