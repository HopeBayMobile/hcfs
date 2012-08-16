from django.conf.urls import patterns, include, url

urlpatterns = patterns("systemlog.views",
    url(r'^$', "index", name="systemlog"),
)
