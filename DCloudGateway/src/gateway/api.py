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


@common.timeout(180)
def _mount(storage_url, key):

	try:
		print "Hello"
		#config = ConfigParser.ConfigParser()
       		#with open('/etc/delta/Gateway.ini','rb') as fh:
		#	config.readfp(fh)

		#if not config.has_section("mountpoint"):
		#	raise MountError("Failed to find mountpoint section in the config file")

		#print config.get("s3ql", "mountOpt")
		#cmd = "mkfs.s3ql swift://%s/gateway/delta"%(storage_url)
		#po  = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		#(stdout, stderr) = po.communicate(key)
        	#if po.returncode != 0:
		#	if stderr.find("existing file system!") == -1:
		#		op_msg = "Failed to mkfs for %s"%stderr
               	#		raise BuildGWError(op_msg)
		#	else:
		#		log.info("Found existing file system!")
	
	except IOError as e:
		op_msg = 'Failed to access /etc/delta/Gateway.ini'
		log.error(str(e))

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
		_mkfs(storage_url=url, key = key)

		op_ok = True
		op_msg = 'Succeeded to build gateway'

	except common.TimeoutError:
		op_msg ="Build Gateway failed due to timeout" 
	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
	except BuildGWError as e:
		op_msg = str(e)
	except MountError as e:
		op_msg = str(e)
	except Exception as e:
		log.error(str(e))

	finally:
		if opt_ok == False:
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
			'msg' = "Failed to store the network information",
			'data' = {}
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


if __name__ == '__main__':
	pass
	#	config = ConfigParser.ConfigParser()
       	#	with open('../../Gateway.ini','rb') as op_fh:
	#		config.readfp(op_fh)

	#	print config.get("s3ql", "mountOpt")

