from celery.task import task
from delta.wizard.api import DeltaWizardTask

@task(base=DeltaWizardTask)
def do_meta_form(data):
    from time import sleep
    sleep(3)
    print data["cluster_name"]
    print data["portal_domain"]
    print data["portal_port"]
    print data["replica_number"]
    hosts = do_meta_form.get_zone_hosts()
    print hosts

    do_meta_form.report_progress(10, True, '3secs for 10% progress', None)
    sleep(5)
    do_meta_form.report_progress(20, True, '5secs for 20% progress', None)
    sleep(30)
    #raise Exception('test task fail')


