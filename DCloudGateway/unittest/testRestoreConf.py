#!/usr/bin/python
# -*- coding: utf-8 -*-

import nose
import simplejson as json
from gateway.SwiftClient import *
from gateway.api_restore_conf import *


class TestRestoreConf:
    '''
    '''
    def setup(self):
        pass

    def teardown(self):
        pass

    @nose.tools.raises(SwiftUploadError)
    def testSwiftClientUploadFileNotExist(self):
        swiftClient = SwiftClient()
        swiftClient.upload('config', '/etc/network/inter')

    @nose.tools.raises(SwiftDeleteError)
    def testSwiftClientDeleteFileNotExist(self):
        swiftClient = SwiftClient()
        swiftClient.delete('config', 'aaaaa')

    def testSwiftClientNormalCase(self):
        swiftClient = SwiftClient()
        nose.tools.ok_(swiftClient.upload('config', '/etc/network/interfaces'))
        nose.tools.ok_(swiftClient.delete('config', '/etc/network/interfaces'))

    def testGetConfNormalCase(self):
        result = json.loads(save_gateway_configuration())
        info = json.loads(get_configuration_backup_info())
        print info
        expectSaveReturnVal = {'result'  : True,
                               'code'    : '100',
                               'msg'     : 'config backup success'}
        expectGetReturnVal = {'result'  : True,
                              'data'    : result['data'],
                              'code'    : '100',
                              'msg'     : None}
        nose.tools.ok_(result['result'],
                       "return value: result is different(%s)"
                       % result['result'])
        nose.tools.eq_(result['code'], expectSaveReturnVal['code'],
                       "return value: code is different(%s)"
                       % result['code'])
        nose.tools.eq_(result['msg'], expectSaveReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % result['msg'])

        nose.tools.ok_(info['result'],
                       "return value: result is different(%s)"
                       % info['result'])
        nose.tools.eq_(info['code'], expectGetReturnVal['code'],
                       "return value: code is different(%s)"
                       % info['code'])
        nose.tools.eq_(info['msg'], expectGetReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % info['msg'])
        nose.tools.eq_(info['data'], expectGetReturnVal['data'],
                       "return value: data is different(%s)"
                       % info['data'])

    def testRestoreConfNormalCase(self):
        result = json.loads(save_gateway_configuration())
        info = json.loads(get_configuration_backup_info())
        restore = json.loads(restore_gateway_configuration())
        expectRestoreReturnVal = {'result'  : True,
                                  'code'    : '100',
                                  'msg'     : None}
        nose.tools.ok_(restore['result'],
                       "return value: result is different(%s)"
                       % restore['result'])
        nose.tools.eq_(restore['code'], expectRestoreReturnVal['code'],
                       "return value: code is different(%s)"
                       % restore['code'])
        nose.tools.eq_(restore['msg'], expectRestoreReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % restore['msg'])

if __name__ == "__main__":
    pass
