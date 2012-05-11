from django.conf.urls.defaults import patterns, url
from DWizard.views import DeltaWizard
from forms import form_1, form_2, form_3
from tasks import task_1, task_2, task_3

class TestWizard(DeltaWizard):
    wizard_step = [
        (form_1, task_1),
        (form_2, task_2),
        (form_3, None)
    ]

urlpatterns = TestWizard.patterns()
