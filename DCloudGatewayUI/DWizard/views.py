from django.contrib.formtools.wizard.views import NamedUrlSessionWizardView, StepsHelper
from django.conf.urls import url, patterns
from django.shortcuts import render_to_response
from django.contrib.formtools.wizard.storage import get_storage
from models import Work

class DeltaWizard(NamedUrlSessionWizardView):
    template_name = 'bootstrap_form.html'
    wizard_step = []
    
    @classmethod
    def as_view(cls, *args, **kwargs):
        form_list = [(step[0].__name__, step[0]) for step in cls.wizard_step]
        named_form = tuple(form_list)        
        return super(DeltaWizard, cls).as_view(named_form, url_name='DWizard:DWizard_step', done_step_name='finished')
    
    @classmethod
    def get_initkwargs(cls, form_list, initial_dict=None,
            instance_dict=None, condition_dict=None, *args, **kwargs):
        
        work, created = Work.objects.get_or_create(work_name=cls.__name__)
        
        kwargs = super(DeltaWizard, cls).get_initkwargs(form_list, initial_dict=None,
            instance_dict=None, condition_dict=None, *args, **kwargs)
        return kwargs

    def dispatch(self, request, *args, **kwargs):
        # add the storage engine to the current wizardview instance
        self.prefix = self.get_prefix(*args, **kwargs)
        self.storage = get_storage(self.storage_name, self.prefix, request,
            getattr(self, 'file_storage', None))
        self.steps = StepsHelper(self)
        
        #set current step from model
        work = Work.objects.get(work_name=self.__class__.__name__)
        self.storage.current_step = work.current_form
        
        response = super(DeltaWizard, self).dispatch(request, *args, **kwargs)

        # update the response (e.g. adding cookies)
        self.storage.update_response(response)
        return response

    @classmethod
    def patterns(cls):
        return patterns('',
            url(r'^(?P<step>.+)/$', cls.as_view(), name='DWizard_step'),
            url(r'^$', cls.as_view(), name='DWizard'),
        )
           
    def process_step(self, form):
        data = self.get_form_step_data(form)
        step_index = self.get_step_index()
        
        #save current step
        work = Work.objects.get(work_name=self.__class__.__name__)
        work.current_form = self.wizard_step[step_index][0].__name__
        work.save()
        
        #get task and execute
        task = self.wizard_step[step_index][1]
        if task:
            task.delay(data)
                
        return data

    def done(self, form_list, **kwargs):
        return render_to_response('done.html')
