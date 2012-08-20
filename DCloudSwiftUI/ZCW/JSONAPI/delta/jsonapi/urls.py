from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("delta.jsonapi.views",
                       url(r'^call$', "call"),
                       url(r'^list$', "list"),
                       url(r'^task/(?P<task_id>)/status$', "task_status"),
                       url(r'^task/(?P<task_id>)/cancel$', "task_cancel"),
)
