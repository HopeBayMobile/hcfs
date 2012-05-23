import os.path
import sys
import csv
import json
import os
import ConfigParser
import common
import subprocess
import time
import errno
import re
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

################################################################################
# Configuration

smb_conf_file = "/etc/samba/smb.conf"
nfs_hosts_allow_file = "/etc/hosts.allow"

default_user_id = "superuser"
default_user_pwd = "superuser"

RUN_CMD_TIMEOUT = 15

CMD_CH_SMB_PWD = "%s/change_smb_pwd.sh"%DIR

LOGFILES = {
            "syslog" : "/var/log/syslog",
            "mount" : "/root/.s3ql/mount.log",
            "fsck" : "/root/.s3ql/fsck.log"
            }

LOG_PARSER = {
             "syslog" : re.compile("^(?P<year>[\d]?)(?P<month>[a-zA-Z]{3})\s+(?P<day>\d\d?)\s(?P<hour>\d\d)\:(?P<minute>\d\d):(?P<second>\d\d)(?:\s(?P<suppliedhost>[a-zA-Z0-9_-]+))?\s(?P<host>[a-zA-Z0-9_-]+)\s(?P<process>[a-zA-Z0-9\/_-]+)(\[(?P<pid>\d+)\])?:\s(?P<message>.+)$"),
             "mount" : re.compile("^(?P<year>[\d]{4})\-(?P<month>[\d]{2})\-(?P<day>[\d]{2})\s+(?P<hour>[\d]{2})\:(?P<minute>[\d]{2}):(?P<second>[\d]{2})\.(?P<ms>[\d]+)\s+(\[(?P<pid>[\d]+)\])\s+(?P<message>.+)$"),
             "fsck" : re.compile("^(?P<year>[\d]{4})\-(?P<month>[\d]{2})\-(?P<day>[\d]{2})\s+(?P<hour>[\d]{2})\:(?P<minute>[\d]{2}):(?P<second>[\d]{2})\.(?P<ms>[\d]+)\s+(\[(?P<pid>[\d]+)\])\s+(?P<message>.+)$"),
             }

# if a keywork match a msg, the msg is belong to the class
# key = level, val = keyword array
KEYWORD_FILTER = {
                  "error_log" : ["error", "exception"], # 0
                  "warning_log" : ["warning"], # 1
                  "info_log" : ["nfs", "cifs" , "."], #2
                  # the pattern . matches any log, 
                  # that is if a log mismatches 0 or 1, then it will be assigned to 2
                  }

LOG_LEVEL = {
            0 : ["error_log", "warning_log", "info_log"],
            1 : ["error_log", "warning_log"],
            2 : ["error_log"],
            }

DEFAULT_SHOW_LOG_LEVEL = 2

enable_log = False

CMD_CHK_STO_CACHE_STATE = "s3qlstat /mnt/cloudgwfiles"

NUM_LOG_LINES = 20

MONITOR_IFACE = "eth1"


################################################################################

class BuildGWError(Exception):
	pass

class EncKeyError(Exception):
	pass

class MountError(Exception):
	pass

class TestStorageError(Exception):
	pass

class GatewayConfError(Exception):
	pass

class UmountError(Exception):
	pass

def getGatewayConfig():
	try:
		config = ConfigParser.ConfigParser()
       		with open('/etc/delta/Gateway.ini','rb') as fh:
			config.readfp(fh)

		if not config.has_section("mountpoint"):
			raise GatewayConfError("Failed to find section [mountpoint] in the config file")

		if not config.has_option("mountpoint", "dir"):
			raise GatewayConfError("Failed to find option 'dir'  in section [mountpoint] in the config file")

		if not config.has_section("network"):
			raise GatewayConfError("Failed to find section [network] in the config file")

		if not config.has_option("network", "iface"):
			raise GatewayConfError("Failed to find option 'iface' in section [network] in the config file")

		if not config.has_section("s3ql"):
			raise GatewayConfError("Failed to find section [s3q] in the config file")

		if not config.has_option("s3ql", "mountOpt"):
			raise GatewayConfError("Failed to find option 'mountOpt' in section [s3q] in the config file")

		if not config.has_option("s3ql", "compress"):
			raise GatewayConfError("Failed to find option 'compress' in section [s3q] in the config file")

		return config
	except IOError as e:
		op_msg = 'Failed to access /etc/delta/Gateway.ini'
		raise GatewayConfError(op_msg)
		
def getStorageUrl():
	log.info("getStorageUrl start")
	storage_url = None

	try:
		config = ConfigParser.ConfigParser()
        	with open('/root/.s3ql/authinfo2') as op_fh:
			config.readfp(op_fh)

		section = "CloudStorageGateway"
		storage_url = config.get(section, 'storage-url').replace("swift://","")
	except Exception as e:
		log.error("Failed to getStorageUrl for %s"%str(e))
	finally:
		log.info("getStorageUrl end")
		return storage_url
		
def get_compression():
	log.info("get_compression start")
	op_ok = False
	op_msg = ''
	op_switch = True

	try:
		config = getGatewayConfig()
		compressOpt = config.get("s3ql", "compress")
		if compressOpt == "false":
			op_switch = False

		op_ok = True
		op_msg = "Succeeded to get_compression"

	except GatewayConfError as e:
		op_msg = str(e)
	except Exception as e:
		op_msg = str(e)
	finally:
		if op_ok == False:
			log.error(op_msg)

		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
                      	      'data'   : {'switch': op_switch}}

		log.info("get_compression end")
		return json.dumps(return_val)

# by Rice
def get_gateway_indicators():

	log.info("get_gateway_indicators start")
	op_ok = False
	op_msg = 'Gateway indocators read failed unexpetcedly.'

	op_network_ok = _check_network()
	op_system_check = _check_system()
	op_flush_inprogress = _check_flush()
	op_dirtycache_nearfull = _check_dirtycache()
	op_HDD_ok= _check_HDD()
	op_NFS_srv = _check_nfs_service()
	op_SMB_srv = _check_smb_service()

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

# check network connecttion from gateway to storage by Rice
def _check_network():
	op_network_ok = False
	log.info("_check_network start")
	try:
                op_config = ConfigParser.ConfigParser()
                with open('/root/.s3ql/authinfo2') as op_fh:
                        op_config.readfp(op_fh)

                section = "CloudStorageGateway"
                op_storage_url = op_config.get(section, 'storage-url').replace("swift://","")
		index = op_storage_url.find(":")
		if index != -1:
			op_storage_url = op_storage_url[0:index]

		cmd ="ping -c 5 %s"%op_storage_url
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

	finally:
		log.info("_check_network end")
		return op_network_ok

# check fsck.s3ql daemon by Rice
def _check_system():
	op_system_check = False
	log.info("_check_system start")

        cmd ="ps aux | grep fsck.s3ql"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode == 0:
                if len(lines) > 2:
                        op_system_check = True
        else:
                op_msg = output

	log.info("_check_system end")
	return op_system_check

# flush check by Rice
def _check_flush():
	op_flush_inprogress = False
	log.info("_check_flush start")
	
	cmd ="sudo s3qlstat /mnt/cloudgwfiles"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("Dirty cache near full: True") !=-1:
                        op_dirtycache_nearfull = True
        else:
                op_msg = output

	log.info("_check_flush end")
	return op_flush_inprogress

# dirty cache check by Rice
def _check_dirtycache():
	op_dirtycache_nearfull = False
        log.info("_check_dirtycache start")

        cmd ="sudo s3qlstat /mnt/cloudgwfiles"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("Dirty cache near full: True") !=-1:
                        op_dirtycache_nearfull = True
        else:
                op_msg = output

        log.info("_check_dirtycache end")
        return op_dirtycache_nearfull

# check disks on gateway by Rice
def _check_HDD():
	op_HDD_ok = False
	log.info("_check_HDD start")

	all_disk = common.getAllDisks()
	nu_all_disk = len(all_disk)
	op_all_disk = 0

	for i in all_disk:
		cmd ="sudo smartctl -a %s"%i

		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
	        po.wait()

#      		if po.returncode == 0:
		if output.find("SMART overall-health self-assessment test result: PASSED") !=-1:
			op_all_disk += 1 
		else:
			op_msg = "%s test result: NOT PASSED"%i
	
	if op_all_disk == len(all_disk):
		op_HDD_ok = True

	log.info("_check_HDD end")
	return op_HDD_ok

# check nfs daemon by Rice
def _check_nfs_service():
	op_NFS_srv = False
	log.info("_check_nfs_service start")

	cmd ="sudo service nfs-kernel-server status"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("running") !=-1:
                        op_NFS_srv = True
        else:
                op_msg = output

	log.info("_check_nfs_service end")
	return op_NFS_srv

# check samba daemon by Rice
def _check_smb_service():
	op_SMB_srv = False
	log.info("_check_smb_service start")	

	cmd ="sudo service smbd status"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
                if output.find("running") !=-1:
                        op_SMB_srv = True
        else:
                op_msg = output

	log.info("_check_smb_service end")
	return op_SMB_srv

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

        if test:
            test_gw_results = json.loads(test_storage_account(storage_url, account, password))
            if test_gw_results['result'] == False:
                return json.dumps(test_gw_results)

	try:
		op_config = ConfigParser.ConfigParser()
		#Create authinfo2 if it doesn't exist
		if not os.path.exists('/root/.s3ql/authinfo2'):
			os.system("mkdir -p /root/.s3ql")
			os.system("touch /root/.s3ql/authinfo2")
                        os.system("chown www-data:www-data /root/.s3ql/authinfo2")
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

		
		#TODO: deal with the case where the key stored in /root/.s3ql/authoinfo2 is Wrong
		key = op_config.get(section, 'bucket-passphrase')
		if key != old_key:
			op_msg = "The old_key is incorrect"
			raise Exception(op_msg)

		_umount()

		storage_url = op_config.get(section, 'storage-url')
		cmd = "s3qladm passphrase %s/gateway/delta"%(storage_url)
		po  = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		(stdout, stderr) = po.communicate(new_key)
		if po.returncode !=0:
			if stdout.find("Wrong bucket passphrase") !=-1:
				op_msg = "The key stored in /root/.s3ql/authoinfo2 is incorrect!"
			else:
				op_msg = "Failed to change enc_key for %s"%stdout
			raise Exception(op_msg)
		
		op_config.set(section, 'bucket-passphrase', new_key)
		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)
		
		op_ok = True
		op_msg = 'Succeeded to apply new user enc key'

	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
		log.error(str(e))
	except UmountError as e:
		op_msg = "Failed to umount s3ql for %s"%str(e)
	except common.TimeoutError as e:
		op_msg = "Failed to umount s3ql in 10 minutes."
	except Exception as e:
		log.error(str(e))
	finally:
		log.info("apply_user_enc_key end")

		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
			      'data'   : {}}

		return json.dumps(return_val)

def _createS3qlConf( storage_url):
	log.info("_createS3qlConf start")
	ret = 1
	try:
		config = getGatewayConfig()
		mountpoint = config.get("mountpoint", "dir")
		mountOpt = config.get("s3ql", "mountOpt")
		iface = config.get("network", "iface")
		compress = "lzma" if config.get("s3ql","compress") == "true" else "none"
		mountOpt = mountOpt + " --compress %s"%compress 

		cmd ='sh %s/createS3qlconf.sh %s %s %s "%s"'%(DIR, iface, "swift://%s/gateway/delta"%storage_url, mountpoint, mountOpt)
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()

		ret = po.returncode
		if ret !=0:
			log.error("Failed to create s3ql config for %s"%output)

	except Exception as e:
		log.error("Failed to create s3ql config for %s"%str(e))
	finally:
		log.info("_createS3qlConf end")
		return ret
			

@common.timeout(180)
def _openContainter(storage_url, account, password):
	log.info("_openContainer start")

	try:
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
	finally:
		log.info("_openContainer end")

@common.timeout(180)
def _mkfs(storage_url, key):
	log.info("_mkfs start")

	try:
		cmd = "python /usr/local/bin/mkfs.s3ql --authfile /root/.s3ql/authinfo2 --max-obj-size 2048 swift://%s/gateway/delta"%(storage_url)
		po  = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdout, stderr) = po.communicate(key)
        	if po.returncode != 0:
			if stderr.find("existing file system!") == -1:
				op_msg = "Failed to mkfs for %s"%stderr
               			raise BuildGWError(op_msg)
			else:
				log.info("Found existing file system!")
	finally:
		log.info("_mkfs end")


@common.timeout(600)
def _umount():
	log.info("_umount start")

	try:
		config = getGatewayConfig()
		mountpoint = config.get("mountpoint", "dir")
		
		if os.path.ismount(mountpoint):
			cmd = "python /usr/local/bin/umount.s3ql %s"%(mountpoint)
			po  = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
			output = po.stdout.read()
			po.wait()
			if po.returncode !=0:
				raise UmountError(output)
	except Exception as e:
		raise UmountError(str(e))
		
	finally:
		log.info("_umount end")

@common.timeout(360)
def _mount(storage_url):
	log.info("_mount start")

	try:
		config = getGatewayConfig()

		mountpoint = config.get("mountpoint", "dir")
		mountOpt = config.get("s3ql", "mountOpt")
		compressOpt = "lzma" if config.get("s3ql", "compress") == "true" else "none"		
		mountOpt = mountOpt + " --compress %s"%compressOpt


		authfile = "/root/.s3ql/authinfo2"

		os.system("mkdir -p %s"%mountpoint)
		
		if os.path.ismount(mountpoint):
			raise BuildGWError("A filesystem is mounted on %s"%mountpoint)

		if _createS3qlConf(storage_url) !=0:
			raise BuildGWError("Failed to create s3ql conf")

		#mount s3ql
		cmd = "python /usr/local/bin/mount.s3ql %s --authfile %s swift://%s/gateway/delta %s"%(mountOpt, authfile, storage_url, mountpoint)
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()
        	if po.returncode != 0:
			if output.find("Wrong bucket passphrase") != -1:
				raise EncKeyError("The input encryption key is wrong!")
			raise BuildGWError(output)

		#mkdir in the mountpoint for smb share
		cmd = "mkdir -p %s/sambashare"%mountpoint
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()
        	if po.returncode != 0:
			raise BuildGWError(output)

                #change the owner of samba share to default smb account
                cmd = "chown superuser:superuser %s/sambashare"%mountpoint
                po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output = po.stdout.read()
                po.wait()
                if po.returncode != 0:
                        raise BuildGWError(output)


		#mkdir in the mountpoint for nfs share
		cmd = "mkdir -p %s/nfsshare"%mountpoint
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()
        	if po.returncode != 0:
			raise BuildGWError(output)

                #change the owner of nfs share to nobody:nogroup
                cmd = "chown nobody:nogroup %s/nfsshare"%mountpoint
                po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output = po.stdout.read()
                po.wait()
                if po.returncode != 0:
                        raise BuildGWError(output)


	except GatewayConfError:
		raise
	except EncKeyError:
		raise
	except Exception as e:
		op_msg = "Failed to mount filesystem for %s"%str(e)
		log.error(str(e))
		raise BuildGWError(op_msg)
		

@common.timeout(360)
def _restartServices():
	log.info("_restartServices start")
	try:
		config = getGatewayConfig()

		cmd = "/etc/init.d/smbd restart"
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.read()
                po.wait()
                if po.returncode != 0:
                        op_msg = "Failed to start samba service for %s."%output
			raise BuildGWError(op_msg)

                cmd = "/etc/init.d/nmbd restart"
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.read()
                po.wait()
                if po.returncode != 0:
                        op_msg = "Failed to start samba service for %s."%output
                        raise BuildGWError(op_msg)

		cmd = "/etc/init.d/nfs-kernel-server restart"
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.read()
                po.wait()
                if po.returncode != 0:
                        op_msg = "Failed to start nfs service for %s."%output
			raise BuildGWError(op_msg)

	except GatewayConfError as e:
		raise e, None, sys.exc_info()[2]

	except Exception as e:
		op_msg = "Failed to restart smb&nfs services for %s"%str(e)
		log.error(str(e))
		raise BuildGWError(op_msg)
	finally:
		log.info("_restartServices start")


def build_gateway(user_key):
	log.info("build_gateway start")

	op_ok = False
	op_msg = 'Failed to apply storage accounts for unexpetced errors.'

	try:

		if not common.isValidEncKey(user_key):
			op_msg = "Encryption Key has to be an alphanumeric string of length between 6~20"		
			raise BuildGWError(op_msg)

		op_config = ConfigParser.ConfigParser()
        	with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		op_config.set(section, 'bucket-passphrase', user_key)
		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)

		url = op_config.get(section, 'storage-url').replace("swift://","")
		account = op_config.get(section, 'backend-login')
		password = op_config.get(section, 'backend-password')
		
		_openContainter(storage_url=url, account=account, password=password)
		_mkfs(storage_url=url, key=user_key)
		_mount(storage_url=url)
		_restartServices()
 
		op_ok = True
		op_msg = 'Succeeded to build gateway'

	except common.TimeoutError:
		op_msg ="Build Gateway failed due to timeout" 
	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
	except EncKeyError as e:
		op_msg = str(e)
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
		output = po.stdout.read()
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

                #Jiahong: Adding restarting nmbd as well
                cmd1 = "/etc/init.d/nmbd restart"
                po1 = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                po1.wait()


		if (po.returncode == 0) and (po1.returncode == 0):
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
			'msg': "Failed to store the network information",
			'data': {}
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
		#fixedGateway = op_config.get('network', 'fixedGateway')
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
			f.write("\nauto eth0\niface eth0 inet static")
			f.write("\naddress %s" % fixedIp)
			f.write("\nnetmask %s\n" % fixedMask)
			f.write("\nauto eth1\niface eth1 inet static")
			f.write("\naddress %s" % ip)
			f.write("\nnetmask %s" % mask)
			f.write("\ngateway %s" % gateway)

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
			f.write("nameserver %s\n" % dns1)

			if dns2 != None:
				f.write("nameserver %s\n" % dns2)

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
                with open(fpath+fname, 'r') as fh:
		    fileReader = csv.reader(fh, delimiter=',', quotechar='"')
                    schedule = []
                    for row in fileReader:
                        schedule.append(row)
                    del schedule[0]   # remove header

	except:
		return_val = {
			'result': False,
			'msg': "Open " + fname + " failed.",
			'data': []
		}
		return json.dumps(return_val)
		
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

    log.info("get_smb_user_list")

    op_ok = False
    op_msg = 'Smb account read failed unexpectedly.'

    try:
        parser = ConfigParser.SafeConfigParser() 
        parser.read(smb_conf_file)
    except ConfigParser.ParsingError, err:
        print err
        op_msg = smb_conf_file + ' is not readable.'
        
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
    log.info("get_storage_account end")
        
    return json.dumps(return_val)

@common.timeout(RUN_CMD_TIMEOUT)
def _chSmbPasswd(username, password):
    cmd = ["sh", CMD_CH_SMB_PWD, username, password]

    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
            
    results = proc.stdout.read()
    ret_val = proc.wait() # 0 : success

    if ret_val != 0:
        log.error("%s"%results)

    return ret_val

def set_smb_user_list (username, password):
    '''
    update username to /etc/samba/smb.conf and call smbpasswd to set password
    '''
    return_val = {
                  'result' : False,
                  'msg' : 'set Smb account failed unexpectedly.',
                  'data' : {} }

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
        log.error("set_smb_user_list fails")
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
        log.info("set_smb_user_list fails")
        return_val['msg'] = 'invalid user, and not in current user list.'
        return json.dumps(return_val)
        
    # ok, set the password
    # notice that only a " " is required btw tokens
     
    try:
        ret = _chSmbPasswd(username=username, password=password)
        if ret != 0:
            return_val['msg'] = 'Failed to change smb passwd'
            return json.dumps(return_val)

    except common.TimeoutError:
        log.error("set_smb_user_list timeout")
        return_val['msg'] = 'Timeout for changing passwd.'
        return json.dumps(return_val)

    #Resetting smb service
    smb_return_val = json.loads(restart_smb_service())
    
    if smb_return_val['result'] == False:
        return_val['msg'] = 'Error in restarting smb service.'
        return json.dumps(return_val) 
    
    # good. all set
    return_val['result'] = True
    return_val['msg'] = 'Success to set smb account and passwd'

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

    log.info("get_nfs_access_ip_list starts")

    try:
        with open(nfs_hosts_allow_file, 'r') as fh:
            for line in fh:
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
        log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)
    
    log.info("get_nfs_access_ip_list end")
        
    return json.dumps(return_val)
    

def set_compression(switch):
	log.info("set_compression start")
	op_ok = False
	op_msg = ''
	op_switch = True

	try:
		config = getGatewayConfig()

		if switch == True:
			config.set("s3ql", "compress", "true")
		elif switch == False:
			config.set("s3ql", "compress", "false")

		else:
			raise Exception("The input argument has to be True or False")

		storage_url = getStorageUrl()
		if storage_url is None:
			raise Exception("Failed to get storage url")

                with open('/etc/delta/Gateway.ini','wb') as op_fh:
                        config.write(op_fh)

		if  _createS3qlConf(storage_url) !=0:
			raise Exception("Failed to create new s3ql config")

		op_ok = True
		op_msg = "Succeeded to set_compression"

	except IOError as e:
		op_msg = str(e)
	except GatewayConfError as e:
		op_msg = str(e)
	except Exception as e:
		op_msg = str(e)
	finally:
		if op_ok == False:
			log.error(op_msg)

		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
                      	      'data'   : {}}

		log.info("set_compression end")
		return json.dumps(return_val)

def set_nfs_access_ip_list (array_of_ip):
    '''
    update to /etc/hosts.allow
    the original ip list will be updated to the new ip list 
    '''

    return_val = {
                  'result' : False,
                  'msg' : 'get NFS access ip list failed unexpectedly.',
                  'data' : {} }


    log.info("set_nfs_access_ip_list starts")

    try:
        # try to get services allowed
        with open(nfs_hosts_allow_file, 'r') as fh:
            for line in fh:
                # skip comment lines and empty lines
                if str(line).startswith("#") or str(line).strip() == None: 
                    continue
            
                arr = str(line).strip().split(":")

                # format error
                if len(arr) < 2:
                    log.info(str(nfs_hosts_allow_file) + " format error")
          
                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

            # got good format
            # key = services allowed, val = ip lists
                services = str(arr[0]).strip()
                iplist = arr[1]
                ips = iplist.strip().split(" ") #
            
            
    except :
        log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)

    # finally, updating the file
    try:
        ofile = open(nfs_hosts_allow_file, 'w')
        output = services + " : " + ", ".join(array_of_ip) + "\n"
        ofile.write(output)
        ofile.close()

        return_val['result'] = True
        return_val['msg'] = "Update ip list successfully"
    except:
        log.info("cannot write to " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot write to " + str(nfs_hosts_allow_file)


    #Resetting nfs service
    nfs_return_val = json.loads(restart_nfs_service())

    if nfs_return_val['result'] == False:
        return_val['result'] = False
        return_val['msg'] = 'Error in restarting NFS service.'
        return json.dumps(return_val)

        
    log.info("get_nfs_access_ip_list end")



    
    return json.dumps(return_val)
    
    

# example schedule: [ [1,0,24,512],[2,0,24,1024], ... ]
def apply_scheduling_rules(schedule):			# by Yen
	# write settings to gateway_throttling.cfg
	fpath = "/etc/delta/"
	fname = "gw_schedule.conf"
	try:
                with open(fpath+fname, "w") as fh:
		    fptr = csv.writer(fh)
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

        os.system("/etc/cron.hourly/hourly_run_this")

	
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
		#print "Please check whether s3qlctrl is installed."
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
	if (bw<256):
		return_val = {
			'result': False,
			'msg': "Uploading bandwidth has to be larger than 256KB/s.",
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
		#print "Please check whether s3qlctrl is installed."
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

################################################################################
def month_number(monthabbr):
    """Return the month number for monthabbr; e.g. "Jan" -> 1."""

    MONTHS = ['',
              'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
              'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
              ]

    index = MONTHS.index(monthabbr)
    return index

def classify_logs (log, keyword_filter=KEYWORD_FILTER):
    '''
    give a log msg and a keyword filter {key=category, val = [keyword]}
    find out which category the log belongs to 
    assume that keywords are not overlaped 
    '''

    for category in sorted(keyword_filter.keys()):

        for keyword in keyword_filter[category]:
            #print "in keyword = " + keyword
            if re.search(keyword, log):
                #print "match"
                return category
            else:
                #print "mismatch"
                pass

    return None

def parse_log (type, log_cnt):
    '''
    parse a log line to log_entry data structure 
    different types require different parser
    if a log line doesn't match the pattern, it return None
    
    #type = syslog
    #log_cnt = "May 10 13:43:46 ubuntu dhclient: bound to 172.16.229.78 -- renewal in 277 seconds."

    #type = mount | fsck # i.e.,g from s3ql
    #log_cnt = "2012-05-07 20:22:22.649 [1666] MainThread: [mount] FUSE main loop terminated."
    '''

    #print "in parsing log"
    #print type
    #print log_cnt

    log_entry = {
                 "category" : "",
                 "timestamp" : "",
                 "msg" : ""
                 }

    pat = LOG_PARSER[type]

    m = pat.match(log_cnt)
    if m == None:
        #print "Not found"
        return None

    #print "match"
    #print m.group()
    #return 

    minute = int(m.group('minute'))
    #print minute

    hour = int(m.group('hour'))
    #print hour

    day = int(m.group('day'))
    #print day

    second = int(m.group('second'))
    #print second

    if len(m.group('month')) == 2:
        month = int(m.group('month'))
    else:
        month = month_number(m.group('month'))
    #print month

    try:
        # syslog has't year info, try to fetch group("year") will cause exception
        year = int(m.group('year'))
    except:
        # any exception, using this year instead
        now = datetime.utcnow()
        year = now.year

    #print year

    msg = m.group('message')
    #print msg

    #now = datetime.datetime.utcnow()

    try:
        timestamp = datetime(year, month, day, hour, minute, second) # timestamp
    except Exception as err:
        #print "datatime error"
        #print Exception
        #print err
        return None

    #print "timestamp = "
    #print timestamp
    #print "msg = "
    #print msg
    log_entry["category"] = classify_logs(msg, KEYWORD_FILTER)
    log_entry["timestamp"] = str(timestamp) #str(timestamp.now()) # don't include ms
    log_entry["msg"] = msg
    return log_entry

##########################
def read_logs(logfiles_dict, offset, num_lines):
    '''
    read all files in logfiles_dict, 
    the log will be reversed since new log line is appended to the tail
    
    and then, each log is parsed into log_entry dict.
    
    the offset = 0 means that the latest log will be selected
    num_lines means that how many lines of logs will be returned, 
              set to "None" for selecting all logs 
    '''

    ret_log_cnt = {}

    for type in logfiles_dict.keys():
        ret_log_cnt[type] = []

        log_buf = []

        try:
            log_buf = [line.strip() for line in open(logfiles_dict[type])]
            log_buf.reverse()

            #print log_buf
            #return {}

            # get the log file content
            #ret_log_cnt[type] = log_buf[ offset : offset + num_lines]

            # parse the log file line by line
            nums = NUM_LOG_LINES
            if num_lines == None:
                nums = len(log_buf) - offset # all
            else:
                nums = num_lines

            for log in log_buf[ offset : offset + nums]:
                log_entry = parse_log(type, log)
                if not log_entry == None: #ignore invalid log line 
                    ret_log_cnt[type].append(log_entry)

        except :
            pass

            #if enable_log:
            #    log.info("cannot parse " + logfiles_dict[type])
            #ret_log_cnt[type] = ["None"]
            #print "cannot parse"

    return ret_log_cnt


##############################    
def storage_cache_usage():
    '''
    read all files in logfiles array, return last n lines
    format:
    
Directory entries:    601
Inodes:               603
Data blocks:          1012
Total data size:      12349.54 MB
After de-duplication: 7156.38 MB (57.95% of total)
After compression:    7082.76 MB (57.35% of total, 98.97% of de-duplicated)
Database size:        0.30 MB (uncompressed)
(some values do not take into account not-yet-uploaded dirty blocks in cache)
Cache size: current: 7146.38 MB, max: 20000.00 MB
Cache entries: current: 1011, max: 250000
Dirty cache status: size: 0.00 MB, entries: 0
Cache uploading: Off

Dirty cache near full: False
    
    '''

    ret_usage = {
              "cloud_storage_usage" :  {
                                    "cloud_data" : 0,
                                    "cloud_data_dedup" : 0,
                                    "cloud_data_dedup_compress" : 0
                                  },
              "gateway_cache_usage"   :  {
                                    "max_cache_size" : 0,
                                    "max_cache_entries" : 0,
                                    "used_cache_size" : 0,
                                    "used_cache_entries" : 0,
                                    "dirty_cache_size" : 0,
                                    "dirty_cache_entries" : 0
                                   }
              }

    # Flush check & DirtyCache check
    try:
        #print CMD_CHK_STO_CACHE_STATE
        args = str(CMD_CHK_STO_CACHE_STATE).split(" ")
        proc = subprocess.Popen(args,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)

        results = proc.stdout.read()
        #print results

        ret_val = proc.wait() # 0 : success
        #proc.kill()

        #print ret_val
        real_cloud_data = 0

        if ret_val == 0: # success
            '''
            very boring string parsing process.
            the format should be fixed. otherwise, won't work (no error checking)  
            '''

            for line in results.split("\n"):
                #print line.strip()
                if line.startswith("Total data size:"):
                    tokens = line.split(":")
                    val = tokens[1].replace("MB", "").strip()
                    #print int(float(val)/1024.0)
                    real_cloud_data = float(val)
                    ret_usage["cloud_storage_usage"]["cloud_data"] = int(float(val) / 1024.0) # MB -> GB 
                    #print val

                if line.startswith("After de-duplication:"):
                    tokens = line.split(":")
                    #print tokens[1]
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup"] = int(float(val)) / 1024 # MB -> GB 
                    #print val

                if line.startswith("After compression:"):
                    tokens = line.split(":")
                    #print tokens[1]
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup_compress"] = int(float(val)) / 1024 # MB -> GB 
                    #print val

                if line.startswith("Cache size: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_size"] = int(float(crt_val)) / 1024 # MB -> GB 
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_size"] = int(float(max_val)) / 1024 # MB -> GB 
                    #print max_val

                if line.startswith("Cache entries: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_entries"] = crt_val
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_entries"] = max_val
                    #print max_val

                if line.startswith("Dirty cache status: size:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    real_cloud_data = real_cloud_data - float(crt_val)
                    ret_usage["gateway_cache_usage"]["dirty_cache_size"] = int(float(crt_val)) / 1024 # MB -> GB 
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["dirty_cache_entries"] = max_val
                    #print max_val
                ret_usage["cloud_storage_usage"]["cloud_data"] = max(int(real_cloud_data / 1024), 0)

    except Exception:
        #print Exception
        #print "exception: %s" % CMD_CHK_STO_CACHE_STATE

        if enable_log:
            log.info(CMD_CHK_STO_CACHE_STATE + " fail")

    return ret_usage

##############################
def get_network_speed(iface_name): # iface_name = eth1
    '''
    call get_network_status to get current nic status
    wait for 1 second, and all again. 
    then calculate difference, i.e., up/down link 
    '''

    ret_val = {
               "uplink_usage" : 0 ,
               "downlink_usage" : 0,
               "uplink_backend_usage" : 0 ,
               "downlink_backend_usage" : 0
               }

    pre_status = get_network_status(iface_name)
    time.sleep(1)
    next_status = get_network_status(iface_name)

    try:
        ret_val["downlink_usage"] = int(int(next_status["recv_bytes"]) - int(pre_status["recv_bytes"])) / 1024 # KB 
        ret_val["uplink_usage"] = int(int(next_status["trans_bytes"]) - int(pre_status["trans_bytes"])) / 1024
    except:
        ret_val["downlink_usage"] = 0 
        ret_val["uplink_usage"] = 0
        

    return ret_val

##############################
def get_network_status(iface_name): # iface_name = eth1
    '''
    so far, cannot get current uplink, downlink numbers,
    use file /proc/net/dev, i.e., current tx, rx instead of

    if the target iface cannot find, all find ifaces will be returned
    '''
    ret_network = {}

    lines = open("/proc/net/dev", "r").readlines()

    columnLine = lines[1]
    _, receiveCols , transmitCols = columnLine.split("|")
    receiveCols = map(lambda a:"recv_" + a, receiveCols.split())
    transmitCols = map(lambda a:"trans_" + a, transmitCols.split())

    cols = receiveCols + transmitCols

    faces = {}
    for line in lines[2:]:
        if line.find(":") < 0: continue
        face, data = line.split(":")
        faceData = dict(zip(cols, data.split()))
        face = face.strip()
        faces[face] = faceData

    try:
        ret_network = faces[iface_name]
    except:
        ret_network = faces # return all
        pass

    return ret_network

################################################################################
def get_gateway_status():
    '''
    report gateway status
    '''

    ret_val = {"result" : True,
               "msg" : "Gateway log & status",
               "data" : { "error_log" : [],
                       "cloud_storage_usage" : 0,
                       "gateway_cache_usage" : 0,
                       "uplink_usage" : 0,
                       "downlink_usage" : 0,
                       "uplink_backend_usage" : 0,
                       "downlink_backend_usage" : 0,
                       "network" : {}
                      }
            }

    if enable_log:
        log.info("get_gateway_status")

    # get logs           
    #ret_log_dict = read_logs(NUM_LOG_LINES)
    ret_val["data"]["error_log"] = read_logs(LOGFILES, 0 , NUM_LOG_LINES)

    # get usage
    usage = storage_cache_usage()
    ret_val["data"]["cloud_storage_usage"] = usage["cloud_storage_usage"]
    ret_val["data"]["gateway_cache_usage"] = usage["gateway_cache_usage"]

    # get network statistics
    network = get_network_speed(MONITOR_IFACE)
    ret_val["data"]["uplink_usage"] = network["uplink_usage"]
    ret_val["data"]["downlink_usage"] = network["downlink_usage"]

    return json.dumps(ret_val)

################################################################################
def get_gateway_system_log (log_level, number_of_msg, category_mask):
    '''
    due to there are a few log src, e.g., mount, fsck, syslog, 
    the number_of_msg will apply to each category.
    so, 3 x number_of_msg of log may return for each type of log {info, err, war)
    '''

    ret_val = { "result" : True,
                "msg" : "gateway system logs",
                "data" : { "error_log" : [],
                           "warning_log" : [],
                           "info_log" : []
                         }
               }

    logs = read_logs(LOGFILES, 0 , None) #query all logs

    for level in LOG_LEVEL[log_level]:
        for logfile in logs: # mount, syslog, ...
            counter = 0  # for each info src, it has number_of_msg returned

            for log in logs[logfile]: # log entries
                if counter >= number_of_msg:
                    break # full, finish this src
                try:
                    if log["category"] == level:
                        ret_val["data"][level].append(log)
                        counter = counter + 1
                    else:
                        pass
                except:
                    pass # any except, skip this line

    return json.dumps(ret_val)

#######################################################


if __name__ == '__main__':
	#print build_gateway("1234567")
	#print apply_user_enc_key("123456", "1234567")
    #print get_gateway_status()
    pass
