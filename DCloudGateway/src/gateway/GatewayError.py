#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift client class can connect to swift and do swift operation



class GatewayError(Exception):
    """
    Base class for exceptions in this module.  
    """
    pass

class BackupError(GatewayError):
    """
    """
    def __init__(self, code = None, msg = None):
        self.code = code
        self.msg =msg
        if self.code is None:
            self.code = '001'
        if self.msg is None:
            self.msg = ReturnCode.backupReturnCodes[self.code]
    def __str__(self):
        return "Backup config error(%s): %s" % (self.code, self.msg)        
    
class CreateMetaDataError(BackupError):
    """
    """
    def __init__(self, msg = None):
        self.code = '002'
        self.msg = msg
        super(FileNotFoundError, self).__init__(self.code, self.msg)
        
class SwiftCommandError(BackupError):
    """
    """
    def __init__(self, msg = None):
        self.code = '003'
        self.msg = msg
        super(FileNotFoundError, self).__init__(self.code, self.msg)
        
class SwiftUploadError(BackupError):
    """
    """
    def __init__(self, msg = None):
        self.code = '004'
        self.msg = msg
        super(FileNotFoundError, self).__init__(self._code, self.msg) 

class ReturnCode():
    backupReturnCodes = {
                  '100' : "success",
                  '101' : "success but not update",
                  '001' : 'config backup is fail!',
                  '002' : 'create metadata file is fail!',
                  '003' : 'swift command is fail!',
                  '004' : 'swift upload container is fail!',
                 }
    
def main(argv = None):
    try:
        raise CreateMetaDataError()
    except CreateMetaDataError as e:
        print e
    
if __name__ == '__main__':
    main()
