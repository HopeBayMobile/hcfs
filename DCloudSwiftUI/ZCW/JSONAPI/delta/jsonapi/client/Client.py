import urllib2
import json


class Client(object):
    def __init__(self, api, apikey=None, secret=None):
        self.api = api
        self.apikey = apikey
        self.secret = secret

        response = urllib2.urlopen(self.api + '/list')
        decoded = json.loads(response.read())

        self.api_functions = decoded

    def request(self, command, args):
        if command not in self.api_functions:
            print 'API list'
            print '========'
            print '/n'.join(self.api_functions.keys())
            raise RuntimeError("ERROR: " + command + " is not in callable api list")

        callable_args = self.api_functions[command].keys()
        for arg in args.keys():
            if arg not in callable_args:
                raise RuntimeError("ERROR: " + arg + " is not valid parameter name for function '" + command + "'")

        required_args = [arg for arg in callable_args if self.api_functions[command][arg]['required'] == True]
        for arg in required_args:
            if arg not in args:
                raise RuntimeError("Error: missing required argument '" + arg + "' for function '" + command + "'")

        call_args = {}
        call_args['function'] = command
        call_args['params'] = json.dumps(args)

        response = urllib2.urlopen(self.api + '/call', call_args)
        decoded = json.loads(response.read())

        if 'data' not in decoded or 'result' not in decoded:
            raise RuntimeError("ERROR: Unable to parse the response")

        if decoded['result'] == False:
            raise RuntimeError("ERROR: " + decoded['msg'])

        response = decoded['data']

        return response
