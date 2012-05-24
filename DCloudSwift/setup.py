import os
import subprocess
from setuptools import setup, find_packages

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)

DATADIR = "/etc/delta"

def isAllDebInstalled(debSrc):
	cmd = "find %s -maxdepth 1 -name \'*.deb\'  "%debSrc
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()
	po.wait()

	if po.returncode !=0:
		print "Failed to execute %s"%cmd
		return -1

	returncode = 0
	devnull = open(os.devnull, "w")
	for line in lines:
		pkgname = line.split('/')[-1].split('_')[0]
		retval = subprocess.call(["dpkg", "-s", pkgname], stdout=devnull, stderr=devnull)
		if retval != 0:
			return False

	devnull.close()
	
	return True

def usage():
	print "usage: python setup.py install\n"
def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

def main():

	os.system("cp Swift.ini src/DCloudSwift/")
	if not isAllDebInstalled("misc/deb_src"):
		os.system("cd misc/deb_src; dpkg -i *.deb")

	setup(
		name = "DCloudSwift",
    		version = "0.1",
    		author = "Cloud Data Team, CTBU, Delta Electronic Inc.",
    		description = ("Delta Inc. CTBU Storage Gateway"),
    		license = "Delta Inc.",
    		keywords = ['deploy', 'swift', 'cloud'],

		packages = find_packages('src'),  # include all packages under src
		package_dir = {'':'src'},   # tell distutils packages are under src
		package_data = {
        	# If any package contains *.txt or *.rst files, include them:
        	'': ['*.txt', '*.rst', '*.sh', "*.ini"],
    		},

		entry_points = {
       			 'console_scripts': [
            				     'dcloud_swift_deploy = DCloudSwift.master.SwiftDeploy:deploy',
					     'dcloud_swift_addNodes = DCloudSwift.master.SwiftDeploy:addNodes',
					     'dcloud_swift_deleteNodes = DCloudSwift.master.SwiftDeploy:deleteNodes'
        		 ]
    		},		

		data_files=[ (DATADIR, ['proxyNodes.sample', 'storageNodes.sample']),  
                           ], 
		
		test_suite='unittest',
		long_description=read('README'),
		classifiers=[
        		"Development Status :: 3 - Alpha",
        		"Topic :: FILESYSTEM",
		],
	)

if __name__ == '__main__':
    main()
