import os
import os.path
import time
from gateway import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

lostfound_dir = "/mnt/cloudgwfiles/lost+found"


def check_lostfound_delete():
    if not os.path.exists(lostfound_dir):
        log.debug("Did not find lost+found dir. Aborting.")
        return

    for files in os.listdir(lostfound_dir):
        fullpath_files = os.path.join(lostfound_dir, files)
        print fullpath_files
        if os.path.exists(fullpath_files):
            os.system('sudo rm -rf %s' % fullpath_files)
        else:
            if not os.path.exists(lostfound_dir):
                log.debug("Did not find lost+found dir. Aborting.")
                return
            else:
                log.debug("passing deleting file %s in lost+found" % files)
        time.sleep(0.5)


################################################################

if __name__ == '__main__':
    log.debug('Start checking for unlinked data in lost+found')
    try:
        check_lostfound_delete()
        log.debug('Finished checking for unlinked data in lost+found')
    except Exception as err:
        log.debug('Error in deleting unlinked data in lost+found')
        log.debug('%s' % str(err))
