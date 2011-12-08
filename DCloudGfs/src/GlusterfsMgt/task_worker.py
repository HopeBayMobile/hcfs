#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import json
import httplib
import datetime
import threading
import atexit

from bottle import run, debug, install, request, response
from bottle import route, error, HTTPError
from bottle.ext import sqlite

import worker_settings

@route('/assign_task', method='POST')
def assign_task(db):
	task_id = request.json['task_id']
	function = request.json['function']
	parameters = request.json['parameters']
	description = request.json['description']

	if not all((task_id, function, parameters, description)):
		raise HTTPError

	db.execute('INSERT OR REPLACE INTO tasks'
			' (task_id, function, parameters, description, status)'
			' VALUES (?,?,?,?,?)', (task_id, function, parameters, description, 'queued'))

	json_resp = {
		'result': True,
		'msg': 'task is assigned',
		'data': 'task_id'
	}

	return json_resp

@route('/task_status/:task_id', method='GET')
def task_status(db, task_id):
	row = db.execute('SELECT function, parameters, description, status FROM tasks WHERE task_id = ?', (task_id,)).fetchone()

	if row is None:
		json_resp = {}
	else:
		(function, parameters, description, status) = row
		json_resp = {
			'result': True,
			'msg': 'task exists',
			'data': {
				'task_id': task_id,
				'status': status,
				'progress': 0,
				'msg': 'message here'
			}
		}

	return json_resp

@error(404)
def mistake(code):
	return 'The parameter passed in has the wrong format!'

import time
def stopwatch(callback):

	def wrapper(*args, **kwargs):
		start = time.time()
		body = callback(*args, **kwargs)
		end = time.time()
		response.headers['X-Exec-Time'] = str(end - start)
		return body
	return wrapper


class WorkerTask(object):
	"""Task base class.

	When called tasks apply the `run` method
	This method must be defined by all tasks
	"""
	hostname = None
	worker_id = None
	task_id = None
	args = None
	kwargs = None

	progress = 0
	retries = 0
	
	def __init__(self, task_id):
		self.task_id = task_id
		self.worker_id = get_worker_id()

	def __call__(self, *args, **kwargs):
		return self.run(*args, **kwargs)

	def run(self, *args, **kwargs):
		"""The body of the task executed by workers. Must be overridden"""
		raise NotImplementedError("Tasks must define the run method.")

	@classmethod
	def update_progress(self, progress=0, message=None):
		"""update running progress to local database and server"""

		# make sure the progress is in range 0 to 100
		progress = max(0, min(progress, 100))

		task_info = {
			'worker_id': self.worker_id,
			'task_id': self.task_id,
			'status': 'running',
			'progress': progress,
			'msg': message or '',
		}

		http_conn = httplib.HTTPConnection(worker_settings.DCM_SERVER_IP)
		http_conn.request(
			'POST',
			'/tasks/report',
			json.dumps(task_info)
		)

	@classmethod
	def delay(self, *args, **kwargs):
		"""Run this baby in the background asynchronously"""
		return self.apply_async(args, kwargs)

	@classmethod
	def apply_async(self, args=None, kwargs=None):
		thread = threading.Thread(target=self.run, args=args, kwargs=kwargs)
		thread.daemon = True
		thread.start()

from functools import wraps
from inspect import getargspec
def task(self, *args, **options):

	def inner_create_task_cls(**options):

		def _create_task_cls(fun):
			base = WorkerTask

			@wraps(fun, assigned=("__module__", "__name__"))
			def run(self, *args, **kwargs):
				return fun(*args, **kwargs)

			run.argspec = getargspec(fun)

			cls_dict = dict(options, run=run,
							__module__=fun.__module__,
							__doc__=fun.__doc__)
			T = type(fun.__name__, (base, ), cls_dict)()
			return T

		return _create_task_cls

	if len(args) == 1 and callable(args[0]):
		return inner_create_task_cls(**options)(*args)
	return inner_create_task_cls(**options)


def db_init():
	conn = sqlite3.connect(worker_settings.TASK_SQLITE_DB) # Warning: This file is created in the current directory
	sql_cmd = "CREATE TABLE IF NOT EXISTS worker_config (id TEXT PRIMARY KEY, timestamp TIMESTAMP);"
	conn.execute(sql_cmd)
	conn.commit()

	sql_cmd = "CREATE TABLE IF NOT EXISTS tasks (task_id TEXT PRIMARY KEY, function TEXT NOT NULL, parameters TEXT, description TEXT, status TEXT NOT NULL);"
	conn.execute(sql_cmd)
	conn.commit()
	conn.close()

def db_execute(sql, param=(), fetchall=False, fetchone=False):
	db_conn = sqlite3.connect(worker_settings.TASK_SQLITE_DB)
	result = db_conn.execute(sql, param)
	if fetchall:
		result = result.fetchall()
	elif fetchone:
		result = result.fetchone()
	db_conn.commit()
	db_conn.close()

	return result

def get_worker_id():
	row = db_execute("SELECT id FROM worker_config", fetchone=True)
	(worker_id,) = row or ('',)
	return worker_id

def worker_register(quit=False):
	api_address = 'http://%s:%s/worker_api' % (worker_settings.WORKER_LISTEN_IP, worker_settings.WORKER_LISTEN_PORT)

	worker_id = get_worker_id()
	max_task = worker_settings.WORKER_MAX_TASK
	
	# disable task queueing from server if quitting
	if quit: max_task = 0

	worker_info = {
		'worker_id': worker_id,
		'api_root': api_address,
		'max_task': max_task,
	}

	# connect to DCDM server to do a registration
	try:
		http_conn = httplib.HTTPConnection(worker_settings.DCM_SERVER_IP)
		http_conn.request(
			'POST',
			'/tasks/worker',
			json.dumps(worker_info)
		)
		resp = http_conn.getresponse()

		if resp.status != 200:
			return 'fail'
		else:
			result = resp.read()
			result = json.loads(result)

			task_id = result['data']['worker_id']

			db_execute("INSERT OR REPLACE INTO worker_config (id, timestamp) VALUES (?,?)", (task_id, datetime.datetime.now()))

			return 'success'
	except Exception:
		return 'fail'
	finally:
		http_conn.close()
		
def worker_quit():
	worker_register(quit=True)

def task_imports():
	try:
		for module in worker_settings.TASK_IMPORTS:
			__import__(module)
		return 'success'
	except:
		import traceback
		print traceback.format_exc()
		return 'fail'

def worker_start():
	db_init()
	
	if task_imports() == 'fail':
		print 'failed to import task modules, please check worker_settings.py'
		return

	if worker_register() == 'fail':
		print 'failed to register worker'
		return
	
	# de-register worker from server if this worker API exit cleanly
	atexit.register(worker_quit)

	debug(True)
	install(stopwatch)
	install(sqlite.Plugin(dbfile=worker_settings.TASK_SQLITE_DB))
	run(host=worker_settings.WORKER_LISTEN_IP, port=worker_settings.WORKER_LISTEN_PORT, server='eventlet', reloader=False)

if __name__ == '__main__':
	worker_start()
