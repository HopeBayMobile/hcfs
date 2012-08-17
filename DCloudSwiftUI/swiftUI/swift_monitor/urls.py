from django.conf.urls import patterns, include, url

urlpatterns = patterns("swift_monitor.views",
    url(r'^$', "index", name="monitor"),
)
