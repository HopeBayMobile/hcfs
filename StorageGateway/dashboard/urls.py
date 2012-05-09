from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("dashboard.views",
    url(r'^$', "index"),
    url(r'^account_configuration$', "account_configuration"),
    url(r'^sync_management$', "sync_management"),
    url(r'^system_log$', "system_log"),
    url(r'^login$', "login")
)
