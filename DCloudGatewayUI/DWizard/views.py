from django.contrib.formtools.wizard.views import NamedUrlSessionWizardView
from django.conf.urls import url, patterns

class DeltaWizard(NamedUrlSessionWizardView):
    wizard_step = []
    
    @classmethod
    def as_view(cls, *args, **kwargs):
        form_list = [(step[0].__name__, step[0]) for step in cls.wizard_step]
        named_form = tuple(form_list)        
        return super(NamedUrlSessionWizardView, cls).as_view(named_form, url_name='DWizard:DWizard_step', done_step_name='finished')
    
    @classmethod
    def patterns(cls):
        return patterns('',
            url(r'^(?P<step>.+)/$', cls.as_view(), name='DWizard_step'),
            url(r'^$', cls.as_view(), name='DWizard'),
        )
        
    def process_step(self, form):
        data = self.get_form_step_data(form)
        step = self.get_step_index()
        
        #get task and execute
        task = self.wizard_step[step][1]
        #task.delay(data)
        task()
        
        return data
