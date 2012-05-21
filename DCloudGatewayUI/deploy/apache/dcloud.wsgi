import os
import os.path
import sys

proj_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(proj_dir)

proj_basedir = os.path.dirname(proj_dir)
sys.path.append(proj_basedir)

os.environ['DJANGO_SETTINGS_MODULE'] = 'GatewayUI.settings'

import django.core.handlers.wsgi
application = django.core.handlers.wsgi.WSGIHandler()
