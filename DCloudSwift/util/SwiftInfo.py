import os
import subprocess


def getStorageNodeIpList():
	'''
	Collect ip list of all storge nodes  
	'''
	cmd = 'cd /etc/swift; swift-ring-builder object.builder'
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	po.wait()

	i = 0
	ipList =[]
	for line in po.stdout.readlines():
		if i > 4:
			ipList.append(line.split()[2])

		i+=1

	return ipList


if __name__ == '__main__':
	print getStorageNodeIpList()
