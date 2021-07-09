#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import getpass
from optparse import OptionParser
import logging
logging.basicConfig(level=logging.INFO)
logging.getLogger('suds.client').setLevel(logging.INFO)
import traceback

from suds.client import Client
from suds.bindings.binding import Binding

from pysimplesoap.client import SoapClient


USERNAME = 'trend\ets_fs_auto'
PASSWD = 'testing!@t2009'

def main(username, passwd):
    url = 'https://10.201.16.10/LabManager/SOAP/LabManager.asmx?wsdl'
    client = Client(url)
    print client
#     
#     authen = client.factory.create('AuthenticationHeader')
#     authen.username = username
#     authen.password = passwd 
#     authen.organizationname = 'TW POC Tool Team'
#     authen.workspacename = 'Main'
# 
#     client.set_options(service='LabManager_x0020_SOAP_x0020_interface', port='LabManager_x0020_SOAP_x0020_interfaceSoap12', soapheaders=authen)
#     client.service.GetSingleConfigurationByName('TMLS_Agent')
    client = SoapClient(wsdl="https://10.201.16.10/LabManager/SOAP/LabManager.asmx?wsdl")
    print dir(client)
    client['AuthenticationHeader'] = {'username': username, 'password':passwd, 'organizationname': 'TW POC Tool Team' , 'workspacename': 'Main'}
    result = client.GetConfiguration(1)
    

def cmd_parse():
    usage = "usage: %prog [options] arg1 arg2"
    parser = OptionParser(usage="usage: %prog [options][arg]")
    parser.add_option('-u', '--username',
                      action='store', 
                      type='string', 
                      dest='username',
                      help='username')
    (options, args) = parser.parse_args()
    
    return options, args
    
if __name__ == '__main__':
    options, args = cmd_parse()
    if options.username:
        passwd = getpass.getpass()
        main(options.username, passwd)
        
    
    
    