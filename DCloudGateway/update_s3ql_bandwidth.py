## auther: CTC Cloud Data Team, Yen.
## remember to add this job to cronjob
import csv
import os
import os.path
import sys
import time
from datetime import datetime

# bw = bandwidth, in kbps
def set_bandwidth(bw):
	nic = "eth1"
	bw = str( bw )
	# clear old tc settings
	cmd = "tc qdisc del dev "+nic+" root"
	os.system(cmd)
	# set tc
	cmd = "tc qdisc add dev "+nic+" root handle 1:0 htb default 10"
	os.system(cmd)
	cmd = "tc class add dev "+nic+" parent 1:0 classid 1:10 htb rate "+bw+"kbps ceil "+bw+"kbps prio 0"
	os.system(cmd)
	# set throttling 8080 port in iptables
	cmd = "iptables -A OUTPUT -t mangle -p tcp --sport 8080 -j MARK --set-mark 10"
	os.system(cmd)
	cmd = "tc filter add dev "+nic+" parent 1:0 prio 0 protocol ip handle 10 fw flowid 1:10"
	os.system(cmd)

## =================================================================================================	
def get_scheduled_bandwidth(weekday, hour, schedule):
	for ii in range(1,len(schedule)):
		row = schedule[ii]
		wd = int(row[0])	## weekday
		if (wd==weekday):   ## weekday match
			sh = int(row[1]);	eh = int(row[2]);  ## start and end hour
			bw = int(row[3])
			if (sh<=eh):   # format 1, e.g 6:00 to 21:00
				if (hour>=sh and hour<=eh):
					return bw
			if (sh>eh):   # format 2, e.g 22:00 to 05:00
				if (hour>=sh or hour<=eh):
					return bw
			
	return -1;  # -1 means not found

def main():

    retry_max = 10;
    while (not os.path.exists('/mnt/cloudgwfiles/nfsshare')) and (retry_max>0):
        time.sleep(1)
        retry_max = retry_max - 1
    if (retry_max <=0): #Failed? No mounted S3ql FS???
        print('Failed to find mounted S3QL FS')
        return
        
	
## =================================================================================================	
# load config file (gateway_throttling.cfg)
## read in CSV file
    fpath = "/etc/delta/"
    fname = "gw_schedule.conf"
    #fileReader = csv.reader(open(fpath+fname, 'r'), delimiter=',', quotechar='"')
    with open('/etc/delta/gw_schedule.conf', 'r') as fh:
        fileReader = csv.reader(fh, delimiter=',', quotechar='"')
        schedule = []
        for row in fileReader:
	    schedule.append(row)

# get current day of week and hour of time
    d = datetime.now()
    weekday = d.isoweekday()
    hour = d.hour

# find the scheduled bandwidth for now
    bw = 1024 * 1024    # set default bandwidth if it is not defined in cfg file
    bw2 = get_scheduled_bandwidth(weekday, hour, schedule)
    if bw2 < 0:
        bw2 = bw
    if (bw2 == 0):		# bandwidth is set to 0 means the client wants to turn-off uploading
	try:
		#cmd = "/usr/local/bin/s3qlctrl uploadoff /mnt/cloudgwfiles"
                cmd = "/etc/delta/uploadoff"
                os.system(cmd)

		print("Turn off s3ql data upload succeeded")
	except:
		print "Please check whether s3qlctrl is installed."

    if bw2>0:   # bandwidth setting is found in the configuration file
	if bw2>=64:		# we set upload speed should be at least 64kbps as default 
		bw = bw2
	
	try:
		set_bandwidth(bw)  # apply scheduled bandwidth
		print("change bandwidth to " + str(bw) + "kB/s succeeded")
		#cmd = "/usr/local/bin/s3qlctrl uploadon /mnt/cloudgwfiles"
                cmd = "/etc/delta/uploadon"
                os.system(cmd)

		print "Turn on s3ql upload."
	except:
		print "Please check whether s3qlctrl is installed."
    return

if __name__ == '__main__':
        main()

