from django.conf.urls import patterns, include, url

urlpatterns = patterns("swift_maintainance.views",
    url(r'^$', "index", name="monitor"),
)
