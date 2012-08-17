from django.shortcuts import redirect
from views import DeltaWizard


class ConfigLoader(object):

    def process_request(self, request):

        if not DeltaWizard.is_finish():
            if "wizard" != request.path.split('/')[1]:
                return redirect('DWIZARD:wizard')
