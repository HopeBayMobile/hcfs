import os
import util
import time
import json
import pymongo
import datetime
from pymongo import Connection
from util import GlobalVar
from SwiftCfg import SwiftMasterCfg

MASTERCFG = SwiftMasterCfg(GlobalVar.MASTERCONF)
MONGODB_HOST = SwiftMasterCfg(GlobalVar.MASTERCONF).getKwparams()['mongodbHost']
MONGODB_PORT = int(SwiftMasterCfg(GlobalVar.MASTERCONF).getKwparams()['mongodbPort'])

def get_mongodb(dbname, host=MONGODB_HOST, port=MONGODB_PORT):
    """return a mongodb database connection."""
    conn = Connection(host=host, port=port)
    return conn[dbname]

if __name__ == '__main__':
    print GlobalVar.MONITOR_MONGODB
    db = get_mongodb(GlobalVar.MONITOR_MONGODB)
    ret = db.stats.insert({"hostname": "ThinkPad"})
    db2 = get_mongodb(GlobalVar.MONITOR_MONGODB)
    ret = db.stats.find_one({"hostname": "ThinkPad"})

    print ret
