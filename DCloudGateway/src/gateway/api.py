import csv
import json
import os
import ConfigParser
import common
import subprocess
import time
import errno
from eventlet import Timeout, sleep


log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

################################################################################
# Configuration

smb_conf_file = "/etc/samba/smb.conf"
nfs_hosts_allow_file = "/etc/hosts.allow"

enable_log = True 

default_user_id = "admin"
default_user_pwd = "admin"

RUN_CMD_TIMEOUT = 15

CMD_CH_SMB_PWD = "%s/change_smb_pwd.sh"%DIR

################################################################################

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

	all_disk = common.getAllDisks()
	nu_all_disk = len(all_disk)
	op_all_disk = 0
	for i in all_disk:
		cmd ="sudo smartctl -a %s"%i

		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
	        po.wait()

       		if po.returncode == 0:
			if output.find("SMART overall-health self-assessment test result: PASSED") !=-1:
				op_all_disk += 1 
		else:
			op_msg = output
	
	if op_all_disk == len(all_disk):
		op_HDD_ok = True
	
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
		cmd ='sh %s/createS3qlconf.sh %s %s %s "%s"'%(DIR, "eth0", "swift://%s/gateway/delta"%storage_url, mountpoint, mountOpt)
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

	finally:
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

	finally:
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

	info_path = "/etc/delta/network.info"
	return_val = {}
	op_ok = False
	op_msg = "Failed to get network information."
	op_config = ConfigParser.ConfigParser()
	network_info = {}
	
	try:
		with open(info_path) as f:
			op_config.readfp(f)
			ip = op_config.get('network', 'ip')
			gateway = op_config.get('network', 'gateway')
			mask = op_config.get('network', 'mask')
			dns1 = op_config.get('network', 'dns1')
			dns2 = op_config.get('network', 'dns2')

		network_info = {
			'ip': ip,
			'gateway': gateway,
			'mask': mask,
			'dns1': dns1,
			'dns2': dns2
		}

		op_ok = True
		op_msg = "Succeeded to get the network information."

	except IOError as e:
		op_ok = False
		op_msg = "Failed to access %s" % info_path
		log.error(op_msg)

	except Exception as e:
		op_ok = False
		log.error(str(e))

	finally:
		return_val = {
			'result': op_ok,
			'msg': op_msg,
			'data': network_info
		}

		log.info("get_network end")
		return json.dumps(return_val)

def apply_network(ip, gateway, mask, dns1, dns2=None):
	log.info("apply_network start")

	ini_path = "/etc/delta/Gateway.ini"
	op_ok = False
	op_msg = "Failed to apply network configuration."
	op_config = ConfigParser.ConfigParser()

	try:
		with open(ini_path) as f:
			op_config.readfp(f)
			if ip == "" or ip == None:
				ip = op_config.get('network', 'ip')
			if gateway == "" or gateway == None:
				gateway = op_config.get('network', 'gateway')
			if mask == "" or mask == None:
				mask = op_config.get('network', 'mask')
			if dns1 == "" or dns1 == None:
				dns1 = op_config.get('network', 'dns1')
			if dns2 == "" or dns2 == None:
				dns2 = op_config.get('network', 'dns2')

			op_ok = True

	except IOError as e:
		op_ok = False
		log.error("Failed to access %s" % ini_path)

	except Exception as e:
		op_ok = False
		log.error(str(e))

	finally:
		if op_ok == False:

			return_val = {
				'result': op_ok,
				'msg': op_msg,
				'data': {}
			}

			return json.dumps(return_val)
		

	if not _storeNetworkInfo(ini_path, ip, gateway, mask, dns1, dns2):

		return_val = {
			'result': False,
			'msg' : "Failed to store the network information",
			'data' : {}
		}

		return json.dumps(return_val)
		

	if _setInterfaces(ip, gateway, mask, ini_path) and _setNameserver(dns1, dns2):
		if os.system("/etc/init.d/networking restart") == 0:
			op_ok = True
			op_msg = "Succeeded to apply the network configuration."
		else:
			op_ok = False
			log.error("Failed to restart the network.")
	else:
		op_ok = False
		log.error(op_msg)

	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}

	log.info("apply_network end")
	return json.dumps(return_val)

def _storeNetworkInfo(ini_path, ip, gateway, mask, dns1, dns2=None):
	op_ok = False
	op_config = ConfigParser.ConfigParser()
	info_path = "/etc/delta/network.info"

	try:
		with open(ini_path) as f:
			op_config.readfp(f)
			op_config.set('network', 'ip', ip)
			op_config.set('network', 'gateway', gateway)
			op_config.set('network', 'mask', mask)
			op_config.set('network', 'dns1', dns1)

			if dns2 != None:
				op_config.set('network', 'dns2', dns2)

		with open(info_path, "wb") as f:
			op_config.write(f)

		op_ok = True
		log.info("Succeeded to store the network information.")

	except IOError as e:
		op_ok = False
		log.error("Failed to store the network information in %s." % info_path)

	except Exception as e:
		op_ok = False
		log.error(str(e))

	finally:
		return op_ok

def _setInterfaces(ip, gateway, mask, ini_path):
	interface_path = "/etc/network/interfaces"
	op_ok = False
	op_config = ConfigParser.ConfigParser()

	try:
                with open(ini_path) as f:
                        op_config.readfp(f)

                fixedIp = op_config.get('network', 'fixedIp')
                fixedMask = op_config.get('network', 'fixedMask')
		fixedGateway = op_config.get('network', 'fixedGateway')
                op_ok = True
		log.info("Succeeded to get the fixed network information.")

        except IOError as e:
		op_ok = False
                log.error("Failed to access %s" % ini_path)
		return op_ok

        except Exception as e:
		op_ok = False
                log.error(str(e))
		return op_ok

	if os.path.exists(interface_path):
		os.system("cp -p %s %s" % (interface_path, interface_path + "_backup"))
	else:
		os.system("touch %s" % interface_path)
		log.warning("File does not exist: %s" % interface_path)

	try:
		with open(interface_path, "w") as f:
			f.write("auto lo\niface lo inet loopback\n")
			f.write("auto eth0\niface eth0 inet static")
			f.write("address %s" % fixedIp)
			f.write("netmask %s" % fixedMask)
			f.write("gateway %s" % fixedGateway)
			f.write("auto eth1\niface eth1 inet static")
			f.write("address %s" % ip)
			f.write("netmask %s" % mask)
			f.write("gateway %s" % gateway)

		op_ok = True
		log.info("Succeeded to set the network configuration")

	except IOError as e:
		op_ok = False
		log.error("Failed to access %s." % interface_path)

		if os.path.exists(interface_path + "_backup"):
			if os.system("cp -p %s %s" % (interface_path + "_backup", interface_path)) != 0:
				log.warning("Failed to recover %s" % interface_path)
			else:
				log.info("Succeeded to recover %s" % interface_path)

	except Exception as e:
		op_ok = False
		log.error(str(e))

	finally:
		return op_ok

def _setNameserver(dns1, dns2=None):
	nameserver_path = "/etc/resolv.conf"
	op_ok = False

	if os.system("cp -p %s %s" % (nameserver_path, nameserver_path + "_backup")) != 0:
		os.system("touch %s" % nameserver_path)
		log.warning("File does not exist: %s" % nameserver_path)

	try:
		with open(nameserver_path, "w") as f:
			f.write("nameserver %s" % dns1)

			if dns2 != None:
				f.write("nameserver %s" % dns2)

		op_ok = True
		log.info("Succeeded to set the nameserver.")

	except IOError as e:
		op_ok = False
		log.error("Failed to access %s." % nameserver_path)

		if os.path.exists(nameserver_path + "_backup"):
			if os.system("cp -p %s %s" % (nameserver_path + "_backup", nameserver_path)) != 0:
				log.warning("Failed to recover %s" % nameserver_path)
			else:
				log.info("Succeeded to recover %s" % nameserver_path)

	except Exception as e:
		op_ok = False
		log.error(str(e))

	finally:
		return op_ok

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

def get_smb_user_list ():
    '''
    read from /etc/samba/smb.conf
    '''

    username = []

    if enable_log:
        log.info("get_smb_user_list")

    op_ok = False
    op_msg = 'Smb account read failed unexpectedly.'

    try:
        parser = ConfigParser.SafeConfigParser() 
        parser.read(smb_conf_file)
    except ConfigParser.ParsingError, err:
        print err
        op_msg = smb_conf_file + ' is not readable.'
        
        if enable_log:
            log.error(op_msg)
            
        username.append(default_user_id) # default
        
        #print "file is not readable"
    else:

        '''
        #read all info
        for section_name in parser.sections():
            print 'Section:', section_name
            print '  Options:', parser.options(section_name)
            #for name, value in parser.items(section_name):
            #    print '  %s = %s' % (name, value)
            #print
        '''
        
        if parser.has_option("cloudgwshare", "valid users"):
            user = parser.get("cloudgwshare", "valid users")
            username = str(user).split(" ") 
        else:
            #print "parser read fail"
            username.append(default_user_id)  # admin as the default user

        op_ok = True
        op_msg = 'Obtained smb account information'

    
    return_val = {
                  'result' : op_ok,
                  'msg' : op_msg,
                  'data' : {'accounts' : username}}
    if enable_log:
        log.info("get_storage_account end")
        
    return json.dumps(return_val)

def set_smb_user_list (username, password):
    '''
    update username to /etc/samba/smb.conf and call smbpasswd to set password
    '''

    return_val = {
                  'result' : False,
                  'msg' : 'set Smb account failed unexpectedly.',
                  'data' : {} }

    if enable_log:
        log.info("set_smb_user_list starts")

    # currently, only admin can use smb
    if str(username).lower() != default_user_id:
        return_val['msg'] = 'invalid user. Only accept ' + default_user_id
        return json.dumps(return_val)
    
    # get current user list
    try:
        current_users = get_smb_user_list ()
        load_userlist = json.loads(current_users)    
        username_arr = load_userlist["data"]["accounts"]
        
        #print username_arr
    except:
        if enable_log:
            log.info("set_smb_user_list fails")
            
        return_val['msg'] = 'cannot read current user list.'
        return json.dumps(return_val)
    
    
    # for new user, add the new user to linux, update smb.conf, and set password
    # TODO: impl.
    
    # admin must in the current user list
    flag = False
    for u in username_arr:
        #print u
        if u == default_user_id:
            flag = True
            
    if flag == False: # should not happen
        if enable_log:
            log.info("set_smb_user_list fails")
        return_val['msg'] = 'invalid user, and not in current user list.'
        return json.dumps(return_val)
        
    # ok, set the password
    # notice that only a " " is required btw tokens
     
    command = CMD_CH_SMB_PWD + " " + password + " " + username
    args = str(command).split(" ")
    
    try:
        with Timeout(RUN_CMD_TIMEOUT):
            #print args
            proc = subprocess.Popen(args,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            
            results = proc.stdout.read()
            ret_val = proc.wait() # 0 : success
            
            #print results
            #print ret_val

    except Timeout:
        #print "Killing long-running %s" % str(args)
        proc.kill()

        if enable_log:
            log.info("set_smb_user_list timeout")
  
        return_val['msg'] = 'Timeout for changing passwd.'
        return json.dumps(return_val)
    
    
    # good. all set
    return_val['result'] = True
    return_val['msg'] = 'Success to set smb account and passwd'

    if enable_log:
        log.info("set_smb_user_list end")

    return json.dumps(return_val)

def get_nfs_access_ip_list ():
    '''
    read from /etc/hosts.allow
    '''

    return_val = {
                  'result' : False,
                  'msg' : 'get NFS access ip list failed unexpectedly.',
                  'data' : { "array_of_ip" : [] } }

    if enable_log:
        log.info("get_nfs_access_ip_list starts")

    try:
        for line in open(nfs_hosts_allow_file, 'r'):
            # skip comment lines and empty lines
            if str(line).startswith("#") or str(line).strip() == None: 
                continue
            
            #print line

            # accepted format:
            # portmap mountd nfsd statd lockd rquotad : 172.16.229.112 172.16.229.136

            arr = str(line).strip().split(":")
            #print arr

            # format error
            if len(arr) < 2:
                if enable_log:
                    log.info(str(nfs_hosts_allow_file) + " format error")
          
                return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                return json.dumps(return_val)

            # got good format
            # key = services allowed, val = ip lists
            services = str(arr[0]).strip()
            iplist = arr[1]
            ips = iplist.strip().split(" ") #
            
            #print services
            #print ips
            
            return_val['result'] = True
            return_val['msg'] = "Get ip list success"
            return_val['data']["array_of_ip"] = ips
            
            #return json.dumps(return_val)
            
    except :
        if enable_log:
            log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)
    
    if enable_log:
        log.info("get_nfs_access_ip_list end")
        
    return json.dumps(return_val)
    
def set_nfs_access_ip_list (array_of_ip):
    '''
    update to /etc/hosts.allow
    the original ip list will be updated to the new ip list 
    '''

    return_val = {
                  'result' : False,
                  'msg' : 'get NFS access ip list failed unexpectedly.',
                  'data' : { "array_of_ip" : [] } }


    if enable_log:
        log.info("set_nfs_access_ip_list starts")

    try:
        # try to get services allowed
        for line in open(nfs_hosts_allow_file, 'r'):
            # skip comment lines and empty lines
            if str(line).startswith("#") or str(line).strip() == None: 
                continue
            
            arr = str(line).strip().split(":")

            # format error
            if len(arr) < 2:
                if enable_log:
                    log.info(str(nfs_hosts_allow_file) + " format error")
          
                return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                return json.dumps(return_val)

            # got good format
            # key = services allowed, val = ip lists
            services = str(arr[0]).strip()
            iplist = arr[1]
            ips = iplist.strip().split(" ") #
            
            #print services
            #print ips
            
            return_val['result'] = True
            return_val['msg'] = "Get ip list success"
            return_val['data']["array_of_ip"] = ips
            
            #return json.dumps(return_val)
            
    except :
        if enable_log:
            log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)

    # finally, updating the file
    try:
        ofile = open(nfs_hosts_allow_file, 'w')
        output = services + " : " + " ".join(array_of_ip)
        ofile.write(output)
        ofile.close()

        return_val['result'] = True
        return_val['msg'] = "Update ip list successfully"
        return_val['data']["array_of_ip"] = " ".join(array_of_ip)
    except:
        if enable_log:
            log.info("cannot write to " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot write to " + str(nfs_hosts_allow_file)
        
    if enable_log:
        log.info("get_nfs_access_ip_list end")



    
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
	

def stop_upload_sync():			# by Yen
	# generate a new rule set and apply it to the gateway
	schedule = []
	for ii in range(1,8):
		schedule.append( [ii,0,24,0] )
		
	try:
		apply_scheduling_rules(schedule)
	except:
		print "Please check whether s3qlctrl is installed."
		return_val = {
			'result': False,
			'msg': "Turn off cache uploading has failed.",
			'data': {}
		}
		return json.dumps(return_val)
	
	return_val = {
		'result': True,
		'msg': "Cache upload has turned off.",
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

	# generate a new rule set and apply it to the gateway
	schedule = []
	for ii in range(1,8):
		schedule.append( [ii,0,24,bw] )
		
	try:
		apply_scheduling_rules(schedule)
	except:
		print "Please check whether s3qlctrl is installed."
		return_val = {
			'result': False,
			'msg': "Turn on cache uploading has failed.",
			'data': {}
		}
		return json.dumps(return_val)
	
	return_val = {
		'result': True,
		'msg': "Cache upload is turned on.",
		'data': {}
	}
	return json.dumps(return_val)


if __name__ == '__main__':
	pass
