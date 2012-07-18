import json
import time
# Assuming all inputs to the functions are JSON objects


def take_snapshot():

    return_val = {'result': True,
                  'msg': 'Snapshot process underway.',
                  'data': {}}
    return json.dumps(return_val)


def set_snapshot_schedule(snapshot_time):

    if snapshot_time >= 0:
        print('Snapshot time is set to %d:00' % snapshot_time)
    else:
        print('Snapshot schedule is disabled')

    return_val = {'result': True,
                  'msg': 'Done setting snapshot schedule.',
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_schedule():

    return_val = {'result': True,
                  'msg': 'Done setting snapshot schedule.',
                  'data': {'snapshot_time': 1}}
    return json.dumps(return_val)


def get_snapshot_list():

    time1 = time.time()
    time2 = time1 + 20
    time3 = time2 + 100

    #Two sample snapshot entries, one finished and one in progress
    snapshots = [{'name': 'demosnapshot', 'start_time': time1, \
                  'finish_time': time2, 'num_files': 100, \
                  'total_size': 100000, 'exposed': True},
                 {'name': 'new_snapshot', 'start_time': time3, \
                  'finish_time': -1, 'num_files': 0, \
                  'total_size': 0, 'exposed': False}]

    return_val = {'result': True,
                  'msg': 'Done getting snapshot list.',
                  'data': {'snapshots': snapshots}}
    return json.dumps(return_val)


def get_snapshot_in_progress():

    return_val = {'result': True,
                  'msg': 'Done getting snapshot in progress.',
                  'data': {'in_progress': "new_snapshot"}}
    return json.dumps(return_val)


def expose_snapshot(to_expose):

    for snapshot in to_expose:
        print('Exposing snapshot (name: %s) as samba share' % snapshot)

    return_val = {'result': True,
                  'msg': 'Finished exposing snapshot.',
                  'data': {}}
    return json.dumps(return_val)


def delete_snapshot(to_delete):

    print('Deleting snapshot (name: %s)' % to_delete)

    return_val = {'result': True,
                  'msg': 'Finished deleting snapshot.',
                  'data': {}}
    return json.dumps(return_val)


def set_snapshot_lifespan(days_to_live):

    print('Lifespan of a snapshot is set to %d days' % days_to_live)

    return_val = {'result': True,
                  'msg': 'Finished setting lifespan of snapshots.',
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_lifespan():

    return_val = {'result': True,
                  'msg': 'Finished setting lifespan of snapshots.',
                  'data': {'days_to_live': 365}}
    return json.dumps(return_val)

# wthung, 2012/7/17
def get_snapshot_last_status():
    return_val = {'result': True,
                  'msg': 'Latest snapshot is successfully finished',
                  'latest_snapshot_time': 12345}
    return json.dumps(return_val)
