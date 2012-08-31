"""
This file demonstrates writing tests using the unittest module. These will pass
when you run "manage.py test".

Replace this with more appropriate tests for your application.
"""

import nose
import sys
import os
import json
import time
import tasks

class TestZcw():
    def setup(self):
        pass
    def test_assign_swift_zid(self):
        """
        Tests that 1 + 1 always equals 2.
        """
        hosts = [
                 {"island": "AA", "hostname": "TPE1AA0232", "rack": 2, "position": 32}, 
                 {"island": "AA", "hostname": "TPE1AA0126", "rack": 1, "position": 26},
                 {"island": "AA", "hostname": "TPE1AA0325", "rack": 3, "position": 25},
                 {"island": "AA", "hostname": "TPE1AA0125", "rack": 1, "position": 25},
                 {"island": "BB", "hostname": "TPE1BB0101", "rack": 1, "position": 01}
                ]

        ret = tasks.assign_swift_zid(hosts, 3)
        nose.tools.ok_(ret[0]["hostname"] == "TPE1AA0125")
        nose.tools.ok_(ret[1]["hostname"] == "TPE1AA0126")
        nose.tools.ok_(ret[2]["hostname"] == "TPE1AA0232")
        nose.tools.ok_(ret[3]["hostname"] == "TPE1AA0325")
        nose.tools.ok_(ret[4]["hostname"] == "TPE1BB0101")
        nose.tools.ok_(ret[0]["zid"] == 1)
        nose.tools.ok_(ret[1]["zid"] == 2)
        nose.tools.ok_(ret[2]["zid"] == 3)
        nose.tools.ok_(ret[3]["zid"] == 4)
        nose.tools.ok_(ret[4]["zid"] == 5)

        hosts = [
                 {"island": "CC", "hostname": "TPE1CC0201", "rack": 2, "position": 1},
                 {"island": "CC", "hostname": "TPE1CC0102", "rack": 1, "position": 2},
                 {"island": "AA", "hostname": "TPE1AA0101", "rack": 1, "position": 1}, 
                 {"island": "AA", "hostname": "TPE1AA0102", "rack": 1, "position": 2},
                 {"island": "AA", "hostname": "TPE1AA0103", "rack": 1, "position": 3},
                 {"island": "AA", "hostname": "TPE1AA0104", "rack": 1, "position": 4},
                 {"island": "BB", "hostname": "TPE1BB0103", "rack": 1, "position": 3},
                 {"island": "BB", "hostname": "TPE1BB0102", "rack": 1, "position": 2},
                ]

        ret = tasks.assign_swift_zid(hosts, 3)
        nose.tools.ok_(ret[0]["hostname"] == "TPE1AA0101")
        nose.tools.ok_(ret[1]["hostname"] == "TPE1AA0102")
        nose.tools.ok_(ret[2]["hostname"] == "TPE1AA0103")
        nose.tools.ok_(ret[3]["hostname"] == "TPE1AA0104")
        nose.tools.ok_(ret[4]["hostname"] == "TPE1BB0102")
        nose.tools.ok_(ret[5]["hostname"] == "TPE1BB0103")
        nose.tools.ok_(ret[6]["hostname"] == "TPE1CC0102")
        nose.tools.ok_(ret[7]["hostname"] == "TPE1CC0201")
        nose.tools.ok_(ret[0]["zid"] == 1)
        nose.tools.ok_(ret[1]["zid"] == 2)
        nose.tools.ok_(ret[2]["zid"] == 3)
        nose.tools.ok_(ret[3]["zid"] == 4)
        nose.tools.ok_(ret[4]["zid"] == 5)
        nose.tools.ok_(ret[5]["zid"] == 1)
        nose.tools.ok_(ret[6]["zid"] == 2)
        nose.tools.ok_(ret[7]["zid"] == 3)

