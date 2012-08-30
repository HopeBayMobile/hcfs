#!/usr/bin/python
# -*- coding: utf-8 -*-

import nose
import subprocess
import sys
import simplejson as json
from http_proxy.api_http_proxy import *


def _check_http_proxy_service():
    """
    Check whether Squid3 is running.

    @rtype: boolean
    @return: True if Squid3 (HTTP proxy) is alive. Otherwise false.
    """

    op_proxy_check = False

    try:
        cmd = "sudo ps aux | grep squid3"
        po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE,
                               stderr = subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode == 0:
            if len(lines) > 2:
                op_proxy_check = True
        else:
            op_proxy_check = False
    except:
        pass

    return op_proxy_check


def _is_a_package_installed(pkg_name):
    """
    Check wheter a package is installed on the system.

    @type  pkg_name: string
    @param pkg_name: The name of a query package.
    @rtype: boolean
    @return: True if the package is installed. Otherwise false.
    """
    cmd = "dpkg -s " + pkg_name
    po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE,
                           stderr = subprocess.STDOUT)
    res = po.stdout.readline()
    po.wait()
    installed = False

    if 'not installed' in res:
        installed = False
    else:
        installed = True

    return installed

class TestHttpProxy:
    '''
    Unit test for api_http_proxy.py
    '''
    def setup(self):
        if not _is_a_package_installed('squid3'):
            print "Squid3 is not installed. It will cause this unit test fail."
            sys.exit()
        pass

    def teardown(self):
        pass

    def test_TurnOffProxy(self):
        '''
        Test turn off http proxy at [set_http_proxy()]
        '''
        res = json.loads(set_http_proxy("off"))
        # ^^^ do turn off http proxy
        expectReturnVal = {'result': True,
                           'code':   '100',
                           'msg':    None}
        nose.tools.ok_(res['result'],
                       "return value: result is different(%s)"
                       % res['result'])
        nose.tools.eq_(res['code'], expectReturnVal['code'],
                       "return value: code is different(%s)"
                       % res['code'])
        nose.tools.eq_(res['msg'], expectReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % res['msg'])
        # ^^^ check return value
        proxy_check = _check_http_proxy_service()
        nose.tools.ok_(not proxy_check, "squid3 did not turned off")
        # ^^^ check service status

    def test_TurnOnProxy(self):
        '''
        Test turn on http proxy at [set_http_proxy()]
        '''
        res = json.loads(set_http_proxy("on"))
        # ^^^ do turn on http proxy
        expectReturnVal = {'result': True,
                           'code':   '100',
                           'msg':    None}
        nose.tools.ok_(res['result'],
                       "return value: result is different(%s)"
                       % res['result'])
        nose.tools.eq_(res['code'], expectReturnVal['code'],
                       "return value: code is different(%s)"
                       % res['code'])
        nose.tools.eq_(res['msg'], expectReturnVal['msg'],
                       "return value: msg is different(%s)"
                       % res['msg'])
        # ^^^ check return value
        proxy_check = _check_http_proxy_service()
        nose.tools.ok_(proxy_check, "squid3 did not turned on")
        # ^^^ check service status

if __name__ == "__main__":
    _is_a_package_installed('squid3')
    pass
