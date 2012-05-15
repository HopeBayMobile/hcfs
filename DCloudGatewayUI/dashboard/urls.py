from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("dashboard.views",
    url(r'^$', "index"),
    url(r'^account$', "account"),
    url(r'^system$', "system"),
    url(r'^sync$', "sync"),
    url(r'^sharefolder$', "sharefolder"),
    url(r'^syslog$', "syslog"),
    url(r'^power$', "power"),
    url(r'^login$', "login")
)
