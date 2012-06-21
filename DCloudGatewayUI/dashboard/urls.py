from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("dashboard.views",
    url(r'^$', "index", name="index"),
    url(r'^account/(?P<action>\w*)$', "account", name="account"),
    url(r'^system/(?P<action>\w*)$', "system", name="system"),
    url(r'^sync/$', "sync", name="sync"),
    url(r'^sharefolder/(?P<action>\w*)$', "sharefolder", name="sharefolder"),
    url(r'^syslog/$', "syslog", name="syslog"),
    url(r'^power/(?P<action>(poweroff|restart){0,1})$', "power", name="power"),
    url(r'^http_proxy/(?P<action>(on|off){1})$', "http_proxy", name="http_proxy"),
    url(r'^system_upgrade/$', "system_upgrade", name="system_upgrade"),
    url(r'^indicator/$', "indicator", name="indicator"),
    url(r'^status/$', "status", name="status"),
    url(r'^cache_usage/$', "cache_usage", name="cache_usage"),
    url(r'^config/(?P<action>(restore|save){0,1})$', "config", name="config"),
)
