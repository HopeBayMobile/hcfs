from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("dashboard.views",
    url(r'^$', "index", name="index"),
    url(r'^account/(?P<action>\w*)$', "account", name="account"),
    url(r'^system/(?P<action>\w*)$', "system", name="system"),
    url(r'^sync/$', "sync", name="sync"),
    url(r'^sharefolder/$', "sharefolder", name="sharefolder"),
    url(r'^syslog/$', "syslog", name="syslog"),
    url(r'^power/$', "power", name="power"),
)
