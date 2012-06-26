#!/usr/bin/python
# -*- coding: utf-8 -*-

import nose
from gateway.SwiftClient import *


class TestBackupToCloud:
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
        nose.tools.ok_(swiftClient.delete('config', 'etc/network/interfaces'))



if __name__ == "__main__":
    pass
