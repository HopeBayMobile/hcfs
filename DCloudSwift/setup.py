import sys
import os

def usage():
	print "usage: python setup.py install\n"

def main():
	if (len(sys.argv) == 2 and sys.argv[1] == 'install'):
			os.system("cp -r ../DCloudSwift /")

        else:
		usage()

if __name__ == '__main__':
	main()
