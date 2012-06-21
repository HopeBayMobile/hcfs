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
        self._code = code
        self._msg =msg
        if self._code is None:
            code = '001'
        if self._msg is None:
            self._msg = ErrorCode.errorCodes[self._code]
    def __str__(self):
        return "Backup config error(%s): %s" % (self._code, self._msg)        
    
class MetaDataError(BackupError):
    """
    """
    def __init__(self, msg = None):
        self._code = '002'
        self._msg = msg
        super(FileNotFoundError, self).__init__('002', self._msg) 
        
        
class ErrorCode():
    errorCodes = {
                  '001' : 'config backup is fail!',
                  '002' : 'create metadata file is fail!',
                 }
    
def main(argv = None):
    try:
        raise FileNotFoundError()
    except FileNotFoundError as e:
        print e
    
if __name__ == '__main__':
    main()
