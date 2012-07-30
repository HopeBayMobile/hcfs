import os
from setuptools import setup, find_packages
CONFDIR='/etc/delta'
AUTHDIR='/root/.s3ql'
SMBDIR='/etc/samba'
NETDIR='/etc/network'
ETCDIR='/etc'
CRONDIR='/etc/cron.hourly'

def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()
def main():
	setup(
		name = "DCloudGateway",
    		version = "0.1",
    		author = "Cloud Data Team, CTBU, Delta Electronic Inc.",
    		author_email = "jiahong.wu@delta.com.tw",
    		description = ("Delta Inc. CTBU Storage Gateway"),
    		license = "Delta Inc.",
    		keywords = ['gateway', 'swift', 'cloud', 'dedup'],

		packages = find_packages('src'),  # include all packages under src
		package_dir = {'':'src'},   # tell distutils packages are under src
		package_data = {
        	# If any package contains *.txt or *.rst files, include them:
        	'': ['*.txt', '*.rst', '*.sh'],
    		},

		data_files=[ (CONFDIR, ['Gateway.ini']),  
                             (ETCDIR, ['config/hosts.deny']),
                             (ETCDIR, ['config/exports'])
                           ], 
		
		test_suite='unittest',
		long_description=read('README'),
		classifiers=[
        		"Development Status :: 3 - Alpha",
        		"Topic :: FILESYSTEM",
		],
	)

        os.system("cp config/rc.local /etc")
        os.system("cp config/crontab /etc")
        os.system("chmod 600 /etc/crontab")
	os.system("sh ./gateway_scripts/createSmbUser.sh superuser")
	os.system("chmod 666 %s/Gateway.ini"%CONFDIR)
        os.system("cp config/interfaces /etc/network/interfaces")
        os.system("cp config/hosts.allow /etc")
        os.system("cp config/gw_schedule.conf %s/"%CONFDIR)
        os.system("cp gateway_scripts/hourly_run_this %s/"%CRONDIR) 
        os.system("cp gateway_scripts/daily_run_this /etc/cron.daily/")
        os.system("cp gateway_scripts/update_bandwidth %s/"%CONFDIR) 
        os.system("cp gateway_scripts/python_code/* %s/"%CONFDIR)
        os.system("cp gateway_scripts/shaping_port_8080.sh %s/"%CONFDIR)
        os.system("cp gateway_scripts/uploadon %s/"%CONFDIR)
        os.system("cp gateway_scripts/uploadoff %s/"%CONFDIR)
        os.system("cp gateway_scripts/check_expired %s/"%CONFDIR)
        os.system("cp gateway_scripts/post-gwstart.conf /etc/init/")
        os.system("cp gateway_scripts/check-gwstart.conf /etc/init/")
        os.system("cp gateway_scripts/reorder_eth.conf /etc/init/")
        os.system("cp gateway_scripts/nmbd.conf /etc/init/")
        os.system("cp gateway_scripts/smbd.conf /etc/init/")
        os.system("cp gateway_scripts/delete_lostfound.conf /etc/init/")
        os.system("cp config/smb.conf %s/"%SMBDIR)
        os.system("cp config/sudoers_delta /etc/sudoers.d/")
        os.system("cp gateway_scripts/snapshot_bot %s/"%CONFDIR)
        os.system("cp gateway_scripts/snapshot_check %s/"%CONFDIR)
        os.system("cp gateway_scripts/wait_network_up %s/"%CONFDIR)
        os.system("cp gateway_scripts/wait_gateway_up %s/"%CONFDIR)
        os.system("cp gateway_scripts/service_restart %s/"%CONFDIR)
        os.system("cp gateway_scripts/hourly_snapshotting %s/"%CONFDIR)
        os.system("cp gateway_scripts/run_background_tasks %s/"%CONFDIR)
        os.system("cp gateway_scripts/reorder_eth.sh %s/"%CONFDIR)
        os.system("cp gateway_scripts/delete_lostfound %s/"%CONFDIR)
        os.system("rm -rf /root/.s3ql/*")
        os.system("chmod -R 777 /root")
        os.system("chown -R www-data:www-data %s"%CONFDIR)
        os.system("cp -rs /usr/bin/*s3ql* /usr/local/bin")

if __name__ == '__main__':
    main()
