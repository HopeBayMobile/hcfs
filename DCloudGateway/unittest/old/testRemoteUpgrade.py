#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import nose
import simplejson as json
from gateway.api_remote_upgrade import *


class TestRemoteUpgrade:
    '''
    '''
    def setup(self):
        #os.system('sudo apt-get install -y apt-show-versions')
        pass

    def teardown(self):
        pass

    def testGetGatewayVer(self):
        os.system("sudo sh -x gateway_downgrade.sh")
        result = json.loads(get_gateway_version())
        print "aaa: %s" % result
        expectRemoteUpgradeReturnVal = {'result'  : True,
                                        'version' : '1.12.0~natty1',
                                        'code'    : '100',
                                        'msg'     : None}
        nose.tools.ok_(result['result'],
                       "return value: result is different(%s)"
                       % result['result'])
        nose.tools.eq_(result['version'], expectRemoteUpgradeReturnVal['version'],
                       "return value: version is different(%s)"
                       % result['version'])
        nose.tools.eq_(result['code'], expectRemoteUpgradeReturnVal['code'],
                       "return value: code is different(%s)"
                       % result['code'])
        nose.tools.eq_(result['msg'], expectRemoteUpgradeReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % result['msg'])

    def testRemoteUpgrade(self):
        os.system("sudo sh -x gateway_downgrade.sh")
        result = json.loads(upgrade_gateway(False))
        expectRemoteUpgradeReturnVal = {'result'  : True,
                                        'code'    : '100',
                                        'msg'     : None}
        nose.tools.ok_(result['result'],
                       "return value: result is different(%s)"
                       % result['result'])
        nose.tools.eq_(result['code'], expectRemoteUpgradeReturnVal['code'],
                       "return value: code is different(%s)"
                       % result['code'])
        nose.tools.eq_(result['msg'], expectRemoteUpgradeReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % result['msg'])

    def testRemoteUpgradeTwice(self):
        os.system("sudo sh -x gateway_downgrade.sh")
        result = json.loads(upgrade_gateway(False))
        expectRemoteUpgradeReturnVal = {'result'  : True,
                                        'code'    : '100',
                                        'msg'     : None}
        nose.tools.ok_(result['result'],
                       "return value: result is different(%s)"
                       % result['result'])
        nose.tools.eq_(result['code'], expectRemoteUpgradeReturnVal['code'],
                       "return value: code is different(%s)"
                       % result['code'])
        nose.tools.eq_(result['msg'], expectRemoteUpgradeReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % result['msg'])
        
        result = json.loads(upgrade_gateway(False))
        expectRemoteUpgradeReturnVal = {'result'  : False,
                                        'code'    : '003',
                                        'msg'     : 'There is no new update.'}
        nose.tools.ok_(not result['result'],
                       "return value: result is different(%s)"
                       % result['result'])
        nose.tools.eq_(result['code'], expectRemoteUpgradeReturnVal['code'],
                       "return value: code is different(%s)"
                       % result['code'])
        nose.tools.eq_(result['msg'], expectRemoteUpgradeReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % result['msg'])

if __name__ == "__main__":
    pass
