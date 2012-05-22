from django.conf.urls.defaults import patterns, url
from DWizard.views import DeltaWizard
from forms import Form_All,Test_Form_All
from tasks import install_task


class InstallWizard(DeltaWizard):
    template_name = 'wizard/form.html'
    doing_template = 'wizard/doing.html'
    done_template = 'wizard/done.html'
    failure_template = 'wizard/failure.html'
    finish_template = 'wizard/finish.html'
    
    wizard_step = [
#        (Form_All, install_task)
        (Test_Form_All, install_task)
    ]
#    wizard_initial = {
#        'Form_All': {
#            'ip_address': '192.168.0.1',
#            'subnet_mask': '255.255.255.0',
#            'default_gateway': '192.168.0.254',
#            'preferred_dns': '8.8.8.8',
#            'alternate_dns': '8.8.4.4',
#
#            'cloud_storage_url': '172.168.288.53:8080'
#        }

    #Testing Usage
    wizard_initial = {
        'Test_Form_All': {
            'new_password': '1234',
            'retype_new_password':'1234',
                     
            'ip_address': '172.16.229.154',
            'subnet_mask': '255.255.255.0',
            'default_gateway': '172.16.229.1',
            'preferred_dns': '8.8.8.8',
            'alternate_dns': '8.8.4.4',

            'cloud_storage_url': '172.16.228.53:8080',
            'cloud_storage_account': 'system:root',
            'cloud_storage_password': 'testpass',
            
            'encryption_key':'123456',
            'confirm_encryption_key':'123456',
        }
    }

urlpatterns = patterns("wizard.views",
    url(r'^welcome$', "welcome"),
)

urlpatterns += InstallWizard.patterns()
