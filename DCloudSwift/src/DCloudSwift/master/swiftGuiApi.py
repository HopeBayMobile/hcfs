import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import sqlite3
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.util import GlobalVar
from util import util
from util.database import NodeInfoDatabaseBroker
from util.database import MaintenanceBacklogDatabaseBroker


class Dashboard:
    def __init__(self):
        self.logger = util.getLogger(name="swiftGuiApi.Dashborad")
    def get_total_capacity(self):
        pass

class Monitoring:
    def __init__(self):
        self.logger = util.getLogger(name="swiftGuiApi.Monitoring")

class Maintenance:
    def __init__(self):
        self.logger = util.getLogger(name="swiftGuiApi.Maintenance")

if __name__ == "__main__":
    pass
