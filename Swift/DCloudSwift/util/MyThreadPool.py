'''
Created on 2012/03/27

@author: CW
'''

import time
from Queue import Queue
from threading import Thread


class Worker(Thread):
	"""Thread executing tasks from a given task queue."""
	def __init__(self, tasks):
		Thread.__init__(self)
		self.tasks = tasks
		self.daemon = True
		self.start()

	def run(self):
		while True:
			func, args, kargs = self.tasks.get()
			try:
				func(*args, **kargs)
			except Exception, e:
				print e
			finally:
				self.tasks.task_done()


class ThreadPool:
	"""Pool of threads consuming tasks from a queue."""
	def __init__(self, num_threads):
		self.tasks = Queue(num_threads)
		for _ in range(num_threads):
			Worker(self.tasks)

	def add_task(self, func, *args, **kargs):
		"""Add a task to queue."""
		self.tasks.put((func, args, kargs))

	def wait_completion(self):
		"""Wait for the completion of all tasks in the queue."""
		self.tasks.join()


if __name__ == '__main__':
	def testPrint(i):
		print "This is begin of %d" % i[0]
		time.sleep(2)
		print "This is end of %d" % i[1]

	pool = ThreadPool(3)

	for i in range(9):
		pool.add_task(testPrint, [i, i+100])

	pool.wait_completion()


