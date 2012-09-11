import sys
import os
import socket
import posixfile
import time
import json
import shlex
import fcntl
import pickle
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)

import maintenance
from util import util
from util import diskUtil
from nodeInstaller import NodeInstaller

Usage = '''
Usage:
    python createRing.py replica_count metadata_dir
arguments:
    [retplica_count] - number of replica
    [] - for updating metadata
    [-s | storage] - for deploying storage node
    [-c | cleanMetadata] - for cleaning metadata
Examples:
    python CmdReceiver.py -p {"password": "deltacloud"}
'''
# The shell script is used to install Swift's packages. (OS: Ubuntu 11.04)

if [ $# != 2 ]; then
        echo "Please enter the correct parameters!"
	echo "For example:"
	echo "If you want to use 3 replicas and your swift config is put in /etc/swift, type the following command."
	echo "./CreateRing.sh 3 /etc/swift"
        exit 1
fi
Replica=$1

mkdir -p $2

cd $2
swift-ring-builder account.builder create 18 $Replica 1
swift-ring-builder container.builder create 18 $Replica 1
swift-ring-builder object.builder create 18 $Replica 1

chown -R swift:swift $2

def main():
    returncode = 0

    try:
        if (len(sys.argv) == 3):
            kwargs = None
            if (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
                kwargs = json.loads(sys.argv[2])
                print 'Proxy deployment start'
                triggerProxyDeploy(**kwargs)
            elif (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
                kwargs = json.loads(sys.argv[2])
                print 'storage deployment start'
                triggerStorageDeploy(**kwargs)
            elif (sys.argv[1] == 'updateMetadata' or sys.argv[1] == '-u'):
                print 'updateMetadata start'
                confDir = sys.argv[2]
                triggerUpdateMetadata(confDir=confDir)
            elif (sys.argv[1] == 'cleanMetadata' or sys.argv[1] == '-c'):
                print 'cleanMetadata start'
                triggerCleanMetadata()
            else:
                print >> sys.stderr, "Usage error: Invalid optins"
                raise UsageError
        else:
            raise UsageError
    except UsageError:
        usage()
        returncode = 1
    except ValueError:
        print >> sys.stderr, "Usage error: Ivalid json format"
        returncode = 1
    except Exception as e:
        print >> sys.stderr, str(e)
        returncode = 1
    finally:
        return returncode

if __name__ == '__main__':
    retcode = 0
    try:
        retcode = main()
    except util.TryLockError as e:
        print >> sys.stderr, str(e)
        retcode = 1
