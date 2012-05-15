from django.conf.urls.defaults import patterns, url
from DWizard.views import DeltaWizard
from forms import Form_All
from tasks import install_task

class InstallWizard(DeltaWizard):
    wizard_step = [
        (Form_All, install_task)
    ]
    wizard_initial = {
                    'Form_All':{'ip_address':'192.168.0.1',
                                'subnet_mask':'255.255.255.0',
                                'default_gateway':'192.168.0.254',
                                'preferred_dns':'8.8.8.8',
                                'alternate_dns':'8.8.4.4',
                                
                                'cloud_storage_url':'http://delta.cloud.storage:8080/'
                                }
                      }

urlpatterns = patterns("wizard.views",
    url(r'^welcome$', "welcome"),
    #url(r'^$', "index"),
    
)
urlpatterns += InstallWizard.patterns()
