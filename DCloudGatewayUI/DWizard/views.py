import json

from django.contrib.formtools.wizard.views import SessionWizardView
from django.conf.urls import url, patterns
from django.shortcuts import render_to_response, redirect
from django.core.exceptions import ObjectDoesNotExist
from django.forms import ValidationError
from django.contrib.formtools.wizard.forms import ManagementForm

from celery.result import AsyncResult
from celery import states

from models import Work, Step


class DeltaWizard(SessionWizardView):
    template_name = 'bootstrap_form.html'
    wizard_step = []
    wizard_initial = {}
    exit_url = '/'

    @classmethod
    def patterns(cls, path=None):
        if path:
            return patterns('',
                url(r'^%s/$' % path, cls.as_view(), name='DWizard'),
            )
        else:
            return patterns('',
                url(r'^$', cls.as_view(), name='DWizard'),
            )

    @classmethod
    def is_going(cls):
        #Initialize work data
        work, created = Work.objects.get_or_create(work_name=cls.__name__)
        try:
            current_step = Step.objects.get(form=work.current_form)
            result = AsyncResult(current_step.task_id)
            if result.status == states.FAILURE:
                return True
        except Step.DoesNotExist:
            pass  # do nothing since no current step exist

        if created:
            return True
        else:
            form_list = [(step[0].__name__) for step in cls.wizard_step]
            if work.current_form == form_list[-1]:
                return False
            else:
                return True

    @classmethod
    def as_view(cls, *args, **kwargs):
        #Initialize work data
        work, created = Work.objects.get_or_create(work_name=cls.__name__)

        form_list = [(step[0].__name__, step[0]) for step in cls.wizard_step]
        named_form = tuple(form_list)
        return super(DeltaWizard, cls).as_view(named_form, initial_dict=cls.wizard_initial)

    def get(self, request, *args, **kwargs):
        #set current step from model
        work = Work.objects.get(work_name=self.__class__.__name__)

        if 'reset' in request.GET:
            # reset all the steps for this wizard
            Step.objects.filter(wizard=work).delete()
            work.current_form = ''
            work.save()
            return redirect('.')

        #first init
        if work.current_form == '':
            self.storage.current_step = self.steps.first
            return self.render(self.get_form())

        self.storage.current_step = work.current_form

        #check if current step have task
        step_index = self.get_step_index()
        task = self.wizard_step[step_index][1]

        if task:
            try:
                step = Step.objects.get(form=self.steps.current)
            except ObjectDoesNotExist:
                return self.render(self.get_form())

            #check task status
            result = AsyncResult(step.task_id)
            if result.state == states.SUCCESS:
                if self.steps.next:
                    if 'next' in request.GET:
                        return render_to_response('done.html')
                    else:
                        self.storage.current_step = self.steps.next
                        return self.render(self.get_form())
                else:
                    meta = result.info
                    return render_to_response('finish.html', {'meta': meta,
                                                              'exit': self.exit_url})
            elif result.state == states.FAILURE:
                meta = result.info
                return render_to_response('failure.html', {'meta': meta})
            else:
                meta = result.info
                return render_to_response('doing.html', {'meta': meta})
        else:
            self.storage.current_step = self.steps.next
            return self.render(self.get_form(), back=True)

    def post(self, *args, **kwargs):
        # Look for a wizard_goto_step element in the posted data which
        # contains a valid step name. If one was found, render the requested
        # form. (This makes stepping back a lot easier).
        wizard_goto_step = self.request.POST.get('wizard_goto_step', None)
        if wizard_goto_step and wizard_goto_step in self.get_form_list():
            self.storage.current_step = wizard_goto_step
            form = self.get_form(
                data=self.storage.get_step_data(self.steps.current),
                files=self.storage.get_step_files(self.steps.current))
            return self.render(form)

        # Check if form was refreshed
        management_form = ManagementForm(self.request.POST, prefix=self.prefix)
        if not management_form.is_valid():
            raise ValidationError(
                'ManagementForm data is missing or has been tampered.')

        form_current_step = management_form.cleaned_data['current_step']
        if (form_current_step != self.steps.current and
                self.storage.current_step is not None):
            # form refreshed, change current step
            self.storage.current_step = form_current_step

        # get the form for the current step
        form = self.get_form(data=self.request.POST, files=self.request.FILES)

        # and try to validate
        if form.is_valid():
            # if the form is valid, store the cleaned data and files.
            self.storage.set_step_data(self.steps.current, self.process_step(form))
            self.storage.set_step_files(self.steps.current, self.process_step_files(form))

            # check if the current step is the last step
            if self.steps.current == self.steps.last:
                # no more steps, render done view
                return self.render_done(form, **kwargs)
            else:
                step_index = self.get_step_index()
                task = self.wizard_step[step_index][1]

                if task:
                    #wait task done
                    return render_to_response('doing.html')
                else:
                    # proceed to the next step
                    return self.render_next_step(form, back=True)
        return self.render(form)

    def process_step(self, form):
        cleaned_data = form.cleaned_data
        step_index = self.get_step_index()

        #save current step
        work = Work.objects.get(work_name=self.__class__.__name__)
        work.current_form = self.wizard_step[step_index][0].__name__
        work.save()

        #get task and execute
        task = self.wizard_step[step_index][1]
        if task:
            result = task.delay(cleaned_data)
            #save task_id to model
            Step.objects.create(wizard=work,
                                form=work.current_form,
                                data=json.dumps(cleaned_data),
                                task_id=result.task_id)
        data = self.get_form_step_data(form)
        return data

    def done(self, form_list, **kwargs):
        step_index = self.get_step_index()
        task = self.wizard_step[step_index][1]

        if task:
            #wait task done
            return render_to_response('doing.html')
        else:
            return render_to_response('done.html')
