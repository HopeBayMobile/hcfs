from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("dashboard.views",
    url(r'^$', "index"),
    url(r'^account_configuration$', "account_configuration"),
    url(r'^login$', "login")
)
