import sys
import os

def usage():
	print "usage: python setup.py install\n"

def main():
	if (len(sys.argv) == 2 and sys.argv[1] == 'install'):
			os.system("cp -r ../DCloudSwift /")
			os.system("rm -rf /DCloudSwift/deb_src")
			os.system("mkdir -p /var/lib/swift")
			os.system("cp -r ../DCloudSwift/deb_src/* /var/lib/swift")

        else:
		usage()

if __name__ == '__main__':
	main()
