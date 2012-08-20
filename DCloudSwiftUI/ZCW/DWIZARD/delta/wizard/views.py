import json
import urllib

from django.contrib.formtools.wizard.views import SessionWizardView
from django.conf.urls import url, patterns
from django.conf import settings
from django.shortcuts import render, redirect
from django.core.exceptions import ObjectDoesNotExist
from django.forms import ValidationError
from django.views.decorators import csrf

from django.contrib.formtools.wizard.forms import ManagementForm
from django.http import HttpResponse

from celery.result import AsyncResult
from celery import states

from models import Work, Step


#Predefined wizard states
class WizardStates:
    #WAIT_USER_INPUT = 'WAIT_USER_INPUT'
    IN_PROGRESS = 'IN_PROGRESS'
    SUCCESS = 'SUCCESS'
    FAIL = 'FAIL'


class DeltaWizardMetaclass(type):
    """
        The metaclass is used to generate template names from settings.py.
    """
    def __new__(cls, name, bases, attrs):
        for templateName, filename in attrs['default_templates'].iteritems():
            #Remove "wizard_" prefix as attribute name,
            #because WizardView must rewrite template_name attribute.
            attributeName = templateName[7:]
            attrs[attributeName] = getattr(settings, templateName.upper(), filename)
        return super(DeltaWizardMetaclass, cls).__new__(cls, name, bases, attrs)


class DeltaWizard(SessionWizardView):

    __metaclass__ = DeltaWizardMetaclass

    #wizard template settings
    default_templates = {
        'wizard_template_name': 'wizard/bootstrap_form.html',
        'wizard_doing_template': 'wizard/doing.html',
        'wizard_done_template': 'wizard/done.html',
        'wizard_failure_template': 'wizard/failure.html',
        'wizard_finish_template': 'wizard/finish.html'
    }
    #meta data for rendering to template
    render_meta = {}
    #page title
    wizard_title = getattr(settings, 'WIZARD_TITLE', 'Delta Wizard')
    #each step is a (form, task) tuple
    wizard_step = getattr(settings, 'WIZARD_STEP', [])
    #when wizard is finished, redirect to this url
    wizard_exit_url = getattr(settings, 'WIZARD_EXIT_URL', '/')

    @classmethod
    def patterns(cls, path=None):
        path_prefix = path + r"/" if path else ""
        return patterns('',
                        url(r'^%s$' % path_prefix, cls.as_view(), name='wizard'),
                        url(r'^%sstatus$' % path_prefix, cls.status, name='status'),
                        url(r'^%snotify$' % path_prefix, csrf.csrf_exempt(cls.notify), name='notify'),
        )

    @classmethod
    def is_finish(cls):
        """
        The function is used to determine a wizard is finished.
        If the final form has a task. It will make sure the task is successful.
        Otherwise, it examine the data in final step.

        True: the wizard is all successful.
        False: the wizard has some steps to do or is failure.
        """
        last_form = cls.wizard_step[-1]

        work, created = Work.objects.get_or_create(work_name=cls.__name__)
        try:
            current_step = Step.objects.get(form=work.current_form)
        except Step.DoesNotExist:
            current_step = None

        if work.current_form == last_form[0].__name__ and current_step:
            if last_form[1]:
                result = AsyncResult(current_step.task_id)
                if result.status == states.SUCCESS:
                    return True
            else:
                if current_step.data:
                    return True
        return False

    @classmethod
    def as_view(cls, *args, **kwargs):
        """
            The view class will get or create the work model when
            the wizard first initialized. Then, the model is assigned
            to work attribute of the class.
        """
        cls.work, created = Work.objects.get_or_create(work_name=cls.__name__)

        form_list = [(step[0].__name__, step[0]) for step in cls.wizard_step]
        if not cls.work.current_form:
            cls.work.current_form = form_list[0][0]
            cls.work.save()
        named_form = tuple(form_list)
        return super(DeltaWizard, cls).as_view(named_form)

    @property
    def task(self):
        step_index = self.get_step_index()
        return self.wizard_step[step_index][1]

    def get(self, request, *args, **kwargs):

        if self.is_finish():
            return self.render_template(self.finish_template)

        try:
            form_step = Step.objects.get(form=self.steps.current)
        except ObjectDoesNotExist:
            form_step = None

        if 'reset' in request.GET and form_step:
            """
            Remove task_id in form_step.
            This implies that the task has executed but not done
            """
            form_step.task_id = None
            form_step.save()
            return redirect('.')

        # read current step from database

        self.storage.current_step = self.work.current_form

        """
        First, we will check the existence of form_step.
        If it is not existed, we assume that it is a new form.

        If the form comes with a task, we will check the task_id to make sure
        the task is executed or not.
        """

        if form_step:
            if form_step.task_id:
                result = AsyncResult(form_step.task_id)
                self.render_meta = result.info
                if result.state == states.SUCCESS:
                    if self.steps.next:
                        if 'next' in request.GET:
                            self.storage.current_step = self.steps.next
                            self.work.current_form = self.storage.current_step
                            self.work.save()
                        else:
                            return self.render_template(self.done_template)
                elif result.state == states.FAILURE:
                    return self.render_template(self.failure_template)
                else:
                    return self.render_template(self.doing_template)
        return self.render(self.get_form())

    def post(self, *args, **kwargs):

        if self.is_finish():
            return self.render_template(self.finish_template)
        # Look for a wizard_goto_step element in the posted data which
        # contains a valid step name. If one was found, render the requested
        # form. (This makes stepping back a lot easier).
        # TODO: We don't consider the case going to the previous form which is
        # handled by the form task.
        wizard_goto_step = self.request.POST.get('wizard_goto_step', None)
        if wizard_goto_step and wizard_goto_step in self.get_form_list():
            self.storage.current_step = wizard_goto_step
            self.work.current_form = wizard_goto_step
            self.work.save()
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

            if self.task:
                # wait task done
                return self.render_template(self.doing_template)
            else:
                # check if the current step is the last step
                if self.steps.current == self.steps.last:
                    # no more steps, render done view
                    return self.render_done(form, **kwargs)
                self.storage.current_step = self.steps.next
                self.work.current_form = self.storage.current_step
                self.work.save()
                return self.render(self.get_form())

        return self.render(form)

    def get_form(self, step=None, data=None, files=None):
        '''
            Get the current_form and insert the initial value
            if the value existed in setp.data. Otherwise, it will
            grab the value from initial_dict which is defined in
            form class.
        '''
        form = super(DeltaWizard, self).get_form(step, data, files)
        try:
            step = Step.objects.get(form=self.steps.current)
            initial_data = json.loads(step.data)
            form.initial = initial_data
        except ObjectDoesNotExist:
            form.initial = getattr(form, "initial_dict", {})

        return form

    def process_step(self, form):
        """
        The function is used to handle form data in origin.
        But, we make it update necessary infomation into DB and
        execute the corresponded task.
        """
        cleaned_data = form.cleaned_data

        # Update the step infomation into DB
        step, create = Step.objects.get_or_create(wizard=self.work,
                                                  form=self.work.current_form)
        step.data = json.dumps(cleaned_data)
        if self.task:
            # Execute the task throught celeryd with cleaned_data
            # as initial_data.
            result = self.task.delay(cleaned_data)
            step.task_id = result.task_id
        step.save()

        data = self.get_form_step_data(form)
        return data

    def done(self, form_list, **kwargs):
        """
        There are only two cases in this function: Doing or Finished.
        """
        if self.task:
            return self.render_template(self.doing_template)
        else:
            return self.render_template(self.finish_template)

    def render_template(self, template):
        #add extra arguments
        return render(self.request, template, {'title': self.wizard_title,
                                               'exit': self.wizard_exit_url,
                                               'meta': self.render_meta,
                                               'is_ajax': self.request.is_ajax()
                                               })

    def render(self, form=None, **kwargs):
        #add extra arguments
        form = form or self.get_form()
        context = self.get_context_data(form=form, **kwargs)
        context['title'] = self.wizard_title
        context['is_ajax'] = self.request.is_ajax()

        # check if previous step have task
        step_index = self.get_step_index()
        if step_index == 0:
            context['can_back'] = False
        else:
            task = self.wizard_step[step_index - 1][1]
            if task:
                context['can_back'] = False
            else:
                context['can_back'] = True

        return self.render_to_response(context)

    @classmethod
    def status(cls, request):
        response = HttpResponse(content_type='application/json')
        result = {
            'result': True,
            'msg': None,
            'data': cls.get_current_state()
        }
        #set status to output response
        response.content = json.dumps(result)
        return response

    @classmethod
    def notify(cls, request):
        from socket import gethostname
        report_wizard_progress_url = 'http://pdcm/zcw/node/%s/wizard_progress' % gethostname()
        state = cls.get_current_state()
        data = json.dumps(state)

        try:
            response = urllib.urlopen(report_wizard_progress_url, data)
        except:
            pass

        return HttpResponse()

    @classmethod
    def get_current_state(cls):
        from socket import gethostname
        state = {'url': 'http://%s:8765/' % gethostname(),
                 'status': None,
                 'msg': None,
                 'steps': [],
        }

        #determine current state of wizard
        form_list = [(step[0].__name__) for step in cls.wizard_step]
        try:
            current_step = Step.objects.get(form=cls.work.current_form)
            result = AsyncResult(current_step.task_id)
            if result.status == states.SUCCESS and cls.work.current_form == form_list[-1]:
                state['status'] = WizardStates.SUCCESS
            elif result.status == states.FAILURE:
                state['status'] = WizardStates.FAIL
            else:
                state['status'] = WizardStates.IN_PROGRESS
        except Step.DoesNotExist:
            state['status'] = WizardStates.IN_PROGRESS

        #add task info into steps
        for form_task in cls.wizard_step:
            form_name = form_task[0].__name__
            task_name = form_task[1].__name__

            if form_task[1]:
                step_info = {}
                step_info['desc'] = task_name

                try:
                    step = Step.objects.get(form=form_name)
                    result = AsyncResult(step.task_id)
                    step_info['progress'] = result.info['progress']
                    msg_list = [(step['msg']) for step in result.info['step_list']]
                    step_info['msg'] = '\n'.join(msg_list)
                    if result.status == states.SUCCESS:
                        step_info['result'] = True
                    elif result.status == states.FAILURE:
                        step_info['result'] = False
                    else:
                        step_info['result'] = None
                except Step.DoesNotExist:
                    step_info['progress'] = 0.0
                    step_info['msg'] = ''
                    step_info['result'] = None

                state['steps'].append(step_info)

        return state
