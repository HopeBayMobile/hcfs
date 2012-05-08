import json
import os
import ConfigParser
import common
import subprocess
import time

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

class BuildGWError(Exception):
	pass

class MountError(Exception):
	pass

class TestStorageError(Exception):
	pass

def get_gateway_indicators():

	log.info("get_gateway_indicators start")
	op_ok = False
	op_msg = 'Gateway indocators read failed unexpetcedly.'
	op_network_ok = False
	op_system_check = False
	op_flush_inprogress = False
	op_dirtycache_nearfull = False
	op_HDD_ok = False
	op_NFS_srv = False
	op_SMB_srv = False

	# Network check

	try:
                op_config = ConfigParser.ConfigParser()
                with open('/root/.s3ql/authinfo2') as op_fh:
                        op_config.readfp(op_fh)

                section = "CloudStorageGateway"
                op_storage_url = op_config.get(section, 'storage-url').replace("swift://","")
		index = op_storage_url.find(":")
		if index != -1:
			op_storage_url = op_storage_url[0:index]

		cmd ="ping %s"%op_storage_url
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()

		if po.returncode == 0:
                	if output.find("cmp_req" and "ttl" and "time") !=-1:
				op_network_ok = True
		else:
			op_msg = output


        except IOError as e:
                op_msg = 'Unable to access /root/.s3ql/authinfo2.'
                log.error(str(e))
        except Exception as e:
                op_msg = 'Unable to obtain storage url or login info.'
                log.error(str(e))
	
	# System check
        cmd ="ps aux | grep fsck.s3ql"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode == 0:
                if len(lines) > 2:
                        op_system_check = True
        else:
                op_msg = output

	# Flush check & DirtyCache check
	cmd ="sudo s3qlstat /mnt/cloudgwfiles"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("Cache uploading: On") !=-1:
                        op_flush_inprogress = True

                if output.find("Dirty cache near full: True") !=-1:
                        op_dirtycache_nearfull = True

        else:
                op_msg = output

	# HDD check

	cmd ="sudo smartctl -a /dev/sda; sudo smartctl -a /dev/sdb"
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

       	if po.returncode == 0:
		if output.find("SMART overall-health self-assessment test result: PASSED") !=-1:
			op_HDD_ok = True
	else:
		op_msg = output

	# NFS service check
	cmd ="sudo service nfs-kernel-server status"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("running") !=-1:
                        op_NFS_srv = True
        else:
                op_msg = output

	# SMB service check
	
	cmd ="sudo service smbd status"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("running") !=-1:
                        op_SMB_srv = True
        else:
                op_msg = output


	op_ok = True
	op_msg = "Gateway indocators read successfully."

	return_val = {'result' : op_ok,
        	      'msg'    : op_msg,
		      'data'   : {'network_ok' : op_network_ok,
				  'system_check' : op_system_check,
				  'flush_inprogress' : op_flush_inprogress,
				  'dirtycache_nearfull' : op_dirtycache_nearfull,
				  'HDD_ok' : op_HDD_ok,
				  'NFS_srv' : op_NFS_srv,
				  'SMB_srv' : op_SMB_srv}}

	log.info("get_gateway_indicators end")
	return json.dumps(return_val)

def get_storage_account():
	log.info("get_storage_account start")

	op_ok = False
	op_msg = 'Storage account read failed unexpectedly.'
	op_storage_url = ''
	op_account = ''

	try:
		op_config = ConfigParser.ConfigParser()
        	with open('/root/.s3ql/authinfo2') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		op_storage_url = op_config.get(section, 'storage-url').replace("swift://","")
		op_account = op_config.get(section, 'backend-login')
		op_ok = True
		op_msg = 'Obtained storage account information'

	except IOError as e:
		op_msg = 'Unable to access /root/.s3ql/authinfo2.'
		log.error(str(e))
	except Exception as e:
		op_msg = 'Unable to obtain storage url or login info.' 
		log.error(str(e))

	return_val = {'result' : op_ok,
		         'msg' : op_msg,
                        'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

	log.info("get_storage_account end")
	return json.dumps(return_val)

def apply_storage_account(storage_url, account, password, test=True):
	log.info("apply_storage_account start")

	op_ok = False
	op_msg = 'Failed to apply storage accounts for unexpetced errors.'

	try:
		op_config = ConfigParser.ConfigParser()
		#Create authinfo2 if it doesn't exist
		if not os.path.exists('/root/.s3ql/authinfo2'):
			os.system("mkdir -p /root/.s3ql")
			os.system("touch /root/.s3ql/authinfo2")
			os.system("chmod 600 /root/.s3ql/authinfo2")

        	with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		if not op_config.has_section(section):
			op_config.add_section(section)

		op_config.set(section, 'storage-url', "swift://%s"%storage_url)
		op_config.set(section, 'backend-login', account)
		op_config.set(section, 'backend-password', password)

		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)
		
		op_ok = True
		op_msg = 'Succeeded to apply storage account'

	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
		log.error(str(e))
	except Exception as e:
		log.error(str(e))

	return_val = {'result' : op_ok,
		      'msg'    : op_msg,
                      'data'   : {}}

	log.info("apply_storage_account end")
	return json.dumps(return_val)

def apply_user_enc_key(old_key=None, new_key=None):
	log.info("apply_user_enc_key start")

	op_ok = False
	op_msg = 'Failed to change encryption keys for unexpetced errors.'

	try:
		#Check if the new key is of valid format
		if not common.isValidEncKey(new_key):
			op_msg = "New encryption Key has to an alphanumeric string of length between 6~20"		
			raise Exception(op_msg)

		op_config = ConfigParser.ConfigParser()
		if not os.path.exists('/root/.s3ql/authinfo2'):
			op_msg = "Failed to find authinfo2"
			raise Exception(op_msg)

       		with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		if not op_config.has_section(section):
			op_msg = "Section CloudStorageGateway is not found."
			raise Exception(op_msg)

		if op_config.has_option(section, 'bucket-passphrase'):
			key = op_config.get(section, 'bucket-passphrase')
			if key is not None and  key != old_key:
				op_msg = "old_key is not correct"
				raise Exception(op_msg)

		op_config.set(section, 'bucket-passphrase', new_key)
		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)
		
		op_ok = True
		op_msg = 'Succeeded to apply new user enc key'

	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
		log.error(str(e))
	except Exception as e:
		log.error(str(e))
	finally:
		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
			      'data'   : {}}

		log.info("apply_user_enc_key end")
		return json.dumps(return_val)

@common.timeout(180)
def _openContainter(storage_url, account, password):

	os.system("touch gatewayContainer.txt")
	cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s upload gateway gatewayContainer.txt"%(storage_url, account, password)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
		op_msg = "Failed to open container for"%output
               	raise BuildGWError(op_msg)
	
	output=output.strip()
	if output != "gatewayContainer.txt":
		op_msg = "Failed to open container for %s"%output
               	raise BuildGWError(op_msg)
	os.system("rm gatewayContainer.txt")

@common.timeout(180)
def _mkfs(storage_url, key):
	cmd = "mkfs.s3ql swift://%s/gateway/delta"%(storage_url)
	po  = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	(stdout, stderr) = po.communicate(key)
        if po.returncode != 0:
		if stderr.find("existing file system!") == -1:
			op_msg = "Failed to mkfs for %s"%stderr
               		raise BuildGWError(op_msg)
		else:
			log.info("Found existing file system!")


@common.timeout(360)
def _mount(storage_url):
	try:
		config = ConfigParser.ConfigParser()
       		with open('/etc/delta/Gateway.ini','rb') as fh:
			config.readfp(fh)

		if not config.has_section("mountpoint"):
			raise BuildGWError("Failed to find section [mountpoint] in the config file")

		if not config.has_option("mountpoint", "dir"):
			raise BuildGWError("Failed to find option 'dir'  in section [mountpoint] in the cofig file")

		mountpoint = config.get("mountpoint", "dir")
		os.system("mkdir -p %s"%mountpoint)
		
		if os.path.ismount(mountpoint):
			raise BuildGWError("A filesystem is mounted on %s"%mountpoint)

		if not config.has_section("s3ql"):
			raise BuildGWError("Failed to find section [s3q] in the config file")

		mountOpt=""
		if config.has_option("s3ql", "mountOpt"):
			mountOpt = config.get("s3ql", "mountOpt")



		authfile = "/root/.s3ql/authinfo2"

		#TODO: get interface from config file
		shellDir = "/usr/lib/delta/gateway_scripts/"
		cmd ='sh %s/createS3qlconf.sh %s %s %s "%s"'%(shellDir, "eth0", "swift://%s/gateway/delta"%storage_url, mountpoint, mountOpt)
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()
        	if po.returncode != 0:
			raise BuildGWError(output)

		cmd = "mount.s3ql %s --authfile %s swift://%s/gateway/delta %s"%(mountOpt, authfile, storage_url, mountpoint)
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()
        	if po.returncode != 0:
			raise BuildGWError(output)

	except IOError as e:
		op_msg = 'Failed to access /etc/delta/Gateway.ini'
		log.error(str(e))
		raise BuildGWError(op_msg)
	except Exception as e:
		op_msg = "Failed to mount filesystem for %s"%str(e)
		log.error(str(e))
		raise BuildGWError(op_msg)
		

def build_gateway():
	log.info("build_gateway start")

	op_ok = False
	op_msg = 'Failed to apply storage accounts for unexpetced errors.'

	try:
		op_config = ConfigParser.ConfigParser()
		#Create authinfo2 if it doesn't exist
        	with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		if not op_config.has_section(section):
			op_config.add_section(section)

		url = op_config.get(section, 'storage-url').replace("swift://","")
		account = op_config.get(section, 'backend-login')
		password = op_config.get(section, 'backend-password')
		key = op_config.get(section, 'bucket-passphrase')

		_openContainter(storage_url=url, account=account, password=password)
		_mkfs(storage_url=url, key=key)
		_mount(storage_url=url)

		op_ok = True
		op_msg = 'Succeeded to build gateway'

	except common.TimeoutError:
		op_msg ="Build Gateway failed due to timeout" 
	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
	except BuildGWError as e:
		op_msg = str(e)
	except Exception as e:
		op_msg = str(e)
	finally:
		if op_ok == False:
			log.error(op_msg)

		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
                      	      'data'   : {}}

		log.info("build_gateway end")
		return json.dumps(return_val)

def restart_nfs_service():
	log.info("restart_nfs_service start")

	return_val = {}
	op_ok = False
	op_msg = "Restarting nfs service failed."

	try:
		cmd = "/etc/init.d/nfs-kernel-server restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode == 0:
			op_ok = True
			op_msg = "Restarting nfs service succeeded."
	except Exception as e:
		op_ok = False
		log.error(str(e))

	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}

	log.info("restart_nfs_service end")
	return json.dumps(return_val)

def restart_smb_service():
	log.info("restart_smb_service start")

	return_val = {}
	op_ok = False
	op_msg = "Restarting samba service failed."

	try:
		cmd = "/etc/init.d/smbd restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode == 0:
			op_ok = True
			op_msg = "Restarting samba service succeeded."
	except Exception as e:
		op_ok = False
		log.error(str(e))

	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}

	log.info("restart_smb_service end")
	return json.dumps(return_val)

def reset_gateway():
	log.info("reset_gateway start")

	return_val = {}
	op_ok = True
	op_msg = "Succeeded to reset the gateway."
	
	pid = os.fork()
	if pid == 0:
		time.sleep(10)
		os.system("reboot")
	else:
		log.info("The gateway will restart after ten seconds.")
		return json.dumps(return_val)

def shutdown_gateway():
	log.info("shutdown_gateway start")
	
	return_val = {}
	op_ok = True
	op_msg = "Succeeded to shutdown the gateway."

	pid = os.fork()
	if pid == 0:
		time.sleep(10)
		os.system("poweroff")
	else:
		log.info("The gateway will shutdown after ten seconds.")
		return json.dumps(return_val)

@common.timeout(180)
def _test_storage_account(storage_url, account, password):
	cmd ="curl -k -v -H 'X-Storage-User: %s' -H 'X-Storage-Pass: %s' https://%s/auth/v1.0"%(account, password, storage_url)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
		op_msg = "Test storage account failed for %s"%output
               	raise TestStorageError(op_msg)

	if not common.isHttp200(output):
		op_msg = "Test storage account failed"
		raise TestStorageError(op_msg)

def test_storage_account(storage_url, account, password):
	log.info("test_storage_account start")

	op_ok = False
	op_msg = 'Test storage account failed for unexpetced errors.'

	try:
		_test_storage_account(storage_url=storage_url, account=account, password=password)
		op_ok = True
		op_msg ='Test storage account succeeded'
			
	except common.TimeoutError:
		op_msg ="Test storage account failed due to timeout" 
		log.error(op_msg)
	except TestStorageError as e:
		op_msg = str(e)
		log.error(op_msg)
	except Exception as e:
		log.error(str(e))
	finally:
		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
			      'data'   : {}}

		log.info("test_storage_account end")
		return json.dumps(return_val)

def get_network():
	log.info("get_network start")
	log.info("get_network end")
	return json.dumps(return_val)

def apply_network(ip, gateway, mask, dns1, dns2=None):
	log.info("apply_network start")

	return_val = {}
	op_ok = False
	op_msg = "Failed to apply network configuration."
	
	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}
	log.info("apply_network end")
	return json.dumps(return_val)


if __name__ == '__main__':
	pass
	print build_gateway()



def get_scheduling_rules():			# by Yen
	# load config file 
	fpath = "/etc/delta/"
	fname = "gw_schedule.conf"
	try:
		fileReader = csv.reader(open(fpath+fname, 'r'), delimiter=',', quotechar='"')
	except:
		return_val = {
			'result': False,
			'msg': "Open " + fname + " failed.",
			'data': []
		}
		return json.dumps(return_val)
		
	schedule = []
	for row in fileReader:
		schedule.append(row)
	del schedule[0]   # remove header

	return_val = {
		'result': True,
		'msg': "Bandwidth throttling schedule is read.",
		'data': schedule
	}
	return json.dumps(return_val)

# example schedule: [ [1,0,24,512],[2,0,24,1024], ... ]
def apply_scheduling_rules(schedule):			# by Yen
	# write settings to gateway_throttling.cfg
	fpath = "/etc/delta/"
	fname = "gw_schedule.conf"
	try:
		fptr = csv.writer(open(fpath+fname, "w"))
		header = ["Day", "Start_Hour", "End_Hour", "Bandwidth (in kB/s)"]
		fptr.writerow(header)
		for row in schedule:
			fptr.writerow(row)
	except:
		return_val = {
			'result': False,
			'msg': "Open " + fname + " to write failed.",
			'data': []
		}
		return json.dumps(return_val)
	
	# apply settings
	os.system("python update_s3ql_bandwidth.py")
	
	return_val = {
		'result': True,
		'msg': "Rules of bandwidth schedule are saved.",
		'data': {}
	}
	return json.dumps(return_val)
	

def force_upload_sync(bw):			# by Yen
	if (bw<64):
		return_val = {
			'result': False,
			'msg': "Uploading bandwidth has to be larger than 64KB/s.",
			'data': {}
		}
		return json.dumps(return_val)
		
	try:
		bw = str( bw )
		# clear old tc settings
		cmd = "tc qdisc del dev eth0 root"
		os.system(cmd)
		# set tc
		cmd = "tc qdisc add dev eth0 root handle 1:0 htb default 10"
		os.system(cmd)
		cmd = "tc class add dev eth0 parent 1:0 classid 1:10 htb rate "+bw+"kbps ceil "+bw+"kbps prio 0"
		os.system(cmd)
		# set throttling 8080 port in iptables
		cmd = "iptables -A OUTPUT -t mangle -p tcp --sport 8080 -j MARK --set-mark 10"
		os.system(cmd)
		cmd = "tc filter add dev eth0 parent 1:0 prio 0 protocol ip handle 10 fw flowid 1:10"
		os.system(cmd)
		print("change bandwidth to " + bw + "kB/s succeeded")
		cmd = "s3qlctrl uploadon /mnt/cloudgwfiles"
		os.system(cmd)
		print "Turn on s3ql upload."
	except:
		print "Please check whether s3qlctrl is installed."
	
	return_val = {
		'result': True,
		'msg': "S3QL upload is turned on.",
		'data': {}
	}
	return json.dumps(return_val)
