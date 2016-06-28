class DataSrc(object):
	def isAvailable(self):	pass

	def get_data(self):
		if self.isAvailable():
			return True, self.fetch()
		else:
			return False, dict()

	def fetch(self):	pass
