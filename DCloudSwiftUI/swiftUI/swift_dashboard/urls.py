from django.conf.urls import patterns, include, url

urlpatterns = patterns("swift_dashboard.views",
    url(r'^$', "index", name="dashboard"),
)
