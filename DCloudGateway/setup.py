import os
from setuptools import setup, find_packages
CONFDIR='/etc/delta'
AUTHDIR='/root/.s3ql'
#SHELLDIR='/usr/lib/delta/gateway_scripts'

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

		data_files=[ (CONFDIR, ['Gateway.ini']), (AUTHDIR, ['authinfo2.template'])], 
		
		test_suite='unittest',
		long_description=read('README'),
		classifiers=[
        		"Development Status :: 3 - Alpha",
        		"Topic :: FILESYSTEM",
		],
	)

	os.system("chmod 600 %s/Gateway.ini"%CONFDIR)
	os.system("chmod 600 %s/authinfo2"%AUTHDIR)

if __name__ == '__main__':
    main()
