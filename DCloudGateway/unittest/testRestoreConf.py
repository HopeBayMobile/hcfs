#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import nose
import simplejson as json
from gateway.SwiftClient import *
from gateway.api_restore_conf import *
from flexmock import flexmock

class TestRestoreConf:
    '''
    '''
    def setup(self):
        reload(subprocess)
        pass

    def teardown(self):
        pass

    @nose.tools.raises(SwiftUploadError)
    def testSwiftClientUploadFileNotExist(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_return(["Local file 'mock/etc/network/inter' not found\n", None])
        swiftClient = SwiftClient()
        swiftClient.upload('config', '/etc/network/inter')

    @nose.tools.raises(SwiftDeleteError)
    def testSwiftClientDeleteFileNotExist(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_return(["Object 'mock/config/aaaaa' not found\n", None])
        swiftClient = SwiftClient()
        swiftClient.delete('config', 'aaaaa')

    @nose.tools.raises(SwiftCommandError)
    def testSwiftClientDeleteCmdFail(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_raise(SwiftCommandError())
        swiftClient = SwiftClient()
        swiftClient.delete('config', 'aaaaa')

    @nose.tools.raises(SwiftCommandError)
    def testSwiftClientUploadCmdFail(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_raise(SwiftCommandError())
        swiftClient = SwiftClient()
        swiftClient.upload('config', 'aaaaa')

    def testSwiftClientNormalCase(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_return(["etc/network/interfaces\n", None])
        swiftClient = SwiftClient()
        nose.tools.ok_(swiftClient.upload('config', '/etc/network/interfaces'))
        self.mock.returncode = '0'
        self.mock.should_receive('communicate').and_return(["etc/network/interfaces\n", None])
        nose.tools.ok_(swiftClient.delete('config', '/etc/network/interfaces'))

    def testBackupToCloudBackup(self):
        self.mock = flexmock()
        flexmock(subprocess.Popen).new_instances(self.mock)
        self.mock.should_receive('wait').and_return(True)
        mock = flexmock()
        mock.should_receive('upload').and_return(True)
        backupConf = BackupToCloud(None, mock)
        datetime = int(backupConf.backup())
        now = int(strftime("%Y%m%d%H%M", gmtime()))
        if now > datetime:
            nose.tools.ok_((now == datetime + 1), "now(%d) > datetime(%d) + 1" % (now, datetime))
        else:
            nose.tools.ok_((now == datetime), "now(%d) != datetime(%d)" % (now, datetime))

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
