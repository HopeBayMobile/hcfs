from django.conf.urls.defaults import patterns, url

urlpatterns = patterns("wizard.views",
    url(r'^$', "index"),
    url(r'^create$', "create")
)
