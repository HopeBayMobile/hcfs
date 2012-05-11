import sys
import os
import subprocess

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)

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

def installAllDeb(debSrc):
	logger = getLogger(name='installDeb')
	cmd = "dpkg -i  %s/*.deb"%debSrc
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
	po.wait()

	if po.returncode !=0:
		print "Failed to execute %s for %s"%(cmd, output)
		return 1

	return 0

def usage():
	print "usage: python setup.py install\n"

def main():
	if (len(sys.argv) == 2 and sys.argv[1] == 'install'):
			if not isAllDebInstalled("../DCloudSwift/misc/deb_src"):
				os.system("cd ../DCloudSwift/misc/deb_src; dpkg -i *.deb")
			os.system("mkdir -p /etc/delta/scripts")
			os.system("rm -rf /etc/delta/scripts/DCloudSwift")
			os.system("cp -r ../DCloudSwift /etc/delta/scripts")
			os.system("mkdir -p /etc/lib/swift")
			os.system("cp -r ../DCloudSwift/misc/deb_src/* /etc/lib/swift")
			os.system("cp -r ../DCloudSwift/misc/BootScripts /etc/lib/swift")
			os.system("rm -rf /etc/delta/scripts/DCloudSwift/misc/deb_src")

        else:
		usage()

if __name__ == '__main__':
	main()
