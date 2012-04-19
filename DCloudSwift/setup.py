import sys
import os

def usage():
	print "usage: python setup.py install\n"

def main():
	if (len(sys.argv) == 2 and sys.argv[1] == 'install'):
			os.system("cp -r ../DCloudSwift /")
			os.system("rm -rf /DCloudSwift/misc/deb_src")
			os.system("mkdir -p /etc/lib/swift")
			os.system("cp -r ../DCloudSwift/misc/deb_src/* /etc/lib/swift")
			os.system("cp -r ../DCloudSwift/misc/BootScripts /etc/lib/swift")

        else:
		usage()

if __name__ == '__main__':
	main()
