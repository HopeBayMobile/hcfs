import os
import sqlite3
import time
import string
import posix_ipc
from threading import Thread
from gateway import quota_db, quota_updater, common
from signal import signal, SIGTERM, SIGINT
from pyinotify import WatchManager, Notifier, ThreadedNotifier, EventsCodes, ProcessEvent
from pyinotify import IN_CLOSE_WRITE, IN_DELETE

log = common.getLogger(name="FSCHG_NOTIFIER", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

g_program_exit = False

class EventProcessor(ProcessEvent):
    def process_IN_CLOSE_WRITE(self, event):
        #print "Create: %s (%s)" %  (os.path.join(event.path, event.name), event.path)
        update_db(event.path)

    def process_IN_DELETE(self, event):
        #print "Remove: %s (%s)" %  (os.path.join(event.path, event.name), event.path)
        update_db(event.path)

def signal_handler(signum, frame):
    """
    SIGTERM signal handler.
    Will set global flag to True to exit program safely
    """

    global g_program_exit
    g_program_exit = True

def _update_to_db(event_path, check_folder_name):
    """
    Check the input event path to see if it's under our checked folders.
    If yes, we update quota DB to set changed bit to 1
    
    @type event_path: string
    @param event_path: The event path passed by pyinotify
    @type check_folder_name: string
    @param check_folder_name: The checked folder name, such as 'nfsshare' and 'sambashare'
    """
    check_folder = quota_db.CLOUD_ROOT_FOLDER + '/' + check_folder_name
    
    if event_path.startswith(check_folder):
        remaining_path = event_path[len(check_folder) + 1:]
        slash_pos = remaining_path.find('/')
        
        subfolder = None
        if slash_pos == -1:
            subfolder = remaining_path
        else:
            subfolder = remaining_path[:slash_pos]
        
        full_path = check_folder + '/' + subfolder
        #print full_path
        quota_db.update(full_path, 1, 'changed')

def update_db(event_path):
    """
    Update quota db by full path of nfsshare or sambashare
    
    @type event_path: string
    @param event_path: Event path passed by pyinotify
    """
    
    try:
        # ensure the path is under nfsshare or sambashare
        _update_to_db(event_path, 'nfsshare')
        _update_to_db(event_path, 'sambashare')
    except Exception as e:
        log.error(str(e))

def thread_mq():
    """
    Worker thread to handle message queue
    """
    global g_program_exit
    
    while not g_program_exit:
        try:
            mq = posix_ipc.MessageQueue(quota_db.MQ_NAME, flags=posix_ipc.O_CREAT)
            s, _ = mq.receive()
            s = s.decode()
            if s == '?monitor_started':
                mq.send('!started')
        except Exception as e:
            log.error(str(e))
        # wait
        time.sleep(1)

def main():
    """
    Main...
    """
    global g_program_exit
    
    # daemonize
    common.daemonize()
    
    # normal exit when killed
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)
    
    # check if quota db and s3ql mount folder both exist
    # if not, sleep for a while and check again
    while not (quota_db.is_exist() and os.path.exists(quota_db.CLOUD_ROOT_FOLDER)):
        for _ in range(10):
            time.sleep(1)
            if g_program_exit:
                return
    
    #print "Creating watch manager..."
    start_time = time.time()
    
    wm = WatchManager()
    mask = IN_DELETE | IN_CLOSE_WRITE  # watched events
    notifier = Notifier(wm, EventProcessor())
    wdd = wm.add_watch(quota_db.CLOUD_ROOT_FOLDER, mask, rec=True, auto_add=True)
    #print("End creating watch manager. Spend %d seconds.." % int(time.time() - start_time))
    #print "Start monitoring..."
    
    # start a thread to handle message queue
    t = Thread(target=thread_mq)
    t.start()
    
    while True:  # loop forever
        try:
            #notifier.loop()
            # process the queue of events as explained above
            notifier.process_events()
            if notifier.check_events():
                # read notified events and enqeue them
                notifier.read_events()
            # you can do some tasks here...
        except KeyboardInterrupt:
            # destroy the inotify's instance on this interrupt (stop monitoring)
            notifier.stop()
            break
        
        #time.sleep(1)
        if g_program_exit:
            break

if __name__ == '__main__':
    main()
