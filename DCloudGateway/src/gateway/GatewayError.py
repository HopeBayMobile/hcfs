#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift client class can connect to swift and do swift operation


class GatewayError(Exception):
    """
    Base class for exceptions in Gateway module.
    """
    pass


class BackupError(GatewayError):
    """
    BackupError class is the base class for backup config to cloud
    and it is inherited from GatewayError.
    BackupError class can be printed and it will print error code
    and error message
    """
    def __init__(self, code=None, msg=None):
        """
        declare BackupError class
        @type code: string
        @param code: error code which you can define,
                     default error code is '001'
        @type msg: string
        @param msg: error message which you can define,
                    default message is default error code message.
        """
        self.code = code
        self.msg = msg
        if self.code is None:
            self.code = '001'
        if self.msg is None:
            self.msg = ErrorCode.BackupErrorCodes[self.code]

    def __str__(self):
        """
        BackupError class can be printed
        """
        return "Backup config error(%s): %s" % (self.code, self.msg)


class CreateMetaDataError(BackupError):
    """
    This class will raise create meta data error.
    """
    def __init__(self, msg=None):
        """
        The error code of CreateMetaDataError is '002'
        """
        self.code = '002'
        self.msg = msg
        super(CreateMetaDataError, self).__init__(self.code, self.msg)


class SwiftCommandError(BackupError):
    """
    This class will raise swift command throw stderr
    """
    def __init__(self, msg=None):
        """
        The error code of SwiftCommandError is '003'
        """
        self.code = '003'
        self.msg = msg
        super(SwiftCommandError, self).__init__(self.code, self.msg)


class SwiftUploadError(BackupError):
    """
    This class raise swift upload config file fail
    """
    def __init__(self, msg=None):
        """
        The error code of SwiftUploadError is '004'
        """
        self.code = '004'
        self.msg = msg
        super(SwiftUploadError, self).__init__(self.code, self.msg)


class ErrorCode():
    """
    This class define backup erro codes
    """
    BackupErrorCodes = {
        '001': 'config backup is fail!',
        '002': 'create metadata file is fail!',
        '003': 'swift command is fail!',
        '004': 'swift upload container is fail!',
    }


def main():
    try:
        raise CreateMetaDataError()
    except CreateMetaDataError as e:
        print e

if __name__ == '__main__':
    main()
