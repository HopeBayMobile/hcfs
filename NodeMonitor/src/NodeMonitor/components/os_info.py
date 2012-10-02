#!/usr/bin/env python

import sys
import os
import subprocess
import json

MODULE_NAME = 'OS_INFO'

class OSInfo:

	def __init__(self):
		self.info = {}

	def find_distrib_info(self):
		"""
		get Ubuntu version information
		@rtype: dict
		@return: id, release, description
		"""

		try:
			f = open("/etc/lsb-release")
		except IOError as e:
			print e

		lines = f.readlines()
		for line in lines:
			key_value = line.strip().partition("=")

			if key_value[0] == "DISTRIB_ID":
				self.info["DISTRIB_ID"] = key_value[2]
			elif key_value[0] == "DISTRIB_RELEASE":
				self.info["DISTRIB_RELEASE"] = key_value[2]
			elif key_value[0] == "DISTRIB_DESCRIPTION":
				self.info["DISTRIB_DESCRIPTION"] = key_value[2]
		return self.info	

	def get_distrib_id(self):
		if not self.info:
			self.find_distrib_info()
		return self.info["DISTRIB_ID"]

	def get_distrib_release(self):
		if not self.info:
			self.find_distrib_info()
		return self.info["DISTRIB_RELEASE"]

	def get_distrib_description(self):
		if not self.info:
			self.find_distrib_info()
		return self.info["DISTRIB_DESCRIPTION"]

def main():
	osInfo = OSInfo()
	print osInfo.find_distrib_info()
	osInfo.info = {}
	print osInfo.get_distrib_description()


if __name__ == '__main__':
	main()
