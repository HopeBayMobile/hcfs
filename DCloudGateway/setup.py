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
			     (SMBDIR, ['config/smb.conf']),
                             (ETCDIR, ['config/hosts.deny']),
                             (ETCDIR, ['config/exports']),
                             (ETCDIR, ['config/rc.local']),
                             (ETCDIR, ['config/crontab'])
                           ], 
		
		test_suite='unittest',
		long_description=read('README'),
		classifiers=[
        		"Development Status :: 3 - Alpha",
        		"Topic :: FILESYSTEM",
		],
	)

	os.system("sh ./gateway_scripts/createSmbUser.sh superuser")
	os.system("chmod 600 %s/Gateway.ini"%CONFDIR)
        os.system("cp config/interfaces /etc/network/interfaces")
        os.system("cp config/hosts.allow /etc")
        os.system("cp config/gw_schedule.conf %s/"%CONFDIR)
        os.system("cp hourly_run_this %s/"%CRONDIR) 
        os.system("cp update_bandwidth %s/"%CONFDIR) 
        os.system("cp update_s3ql_bandwidth.py %s/"%CONFDIR)
        os.system("cp shaping_port_8080.sh %s/"%CONFDIR)
        os.system("cp uploadon %s/"%CONFDIR)
        os.system("cp uploadoff %s/"%CONFDIR)
        os.system("chmod -R 777 /root")


if __name__ == '__main__':
    main()
