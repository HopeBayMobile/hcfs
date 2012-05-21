from django.conf.urls import patterns, include, url
from django.contrib.staticfiles.urls import staticfiles_urlpatterns

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()

urlpatterns = patterns('',
    # Examples:
    # url(r'^$', 'gateway.views.home', name='home'),
    # url(r'^gateway/', include('gateway.foo.urls')),

    # Uncomment the admin/doc line below to enable admin documentation:
    # url(r'^admin/doc/', include('django.contrib.admindocs.urls')),

    # Uncomment the next line to enable the admin:
    # url(r'^admin/', include(admin.site.urls)),
    url(r'^$', 'views.home', name='home'),
    url(r'^login$', 'views.login', name='login'),
    url(r'^logout$', 'views.logout', name='logout'),
    url(r'^wizard/', include('wizard.urls', namespace="wizard")),
    url(r'^dashboard/', include('dashboard.urls', namespace="dashboard")),
    url(r'^dwizard/', include('DWizard.urls', namespace="DWizard")),
) + staticfiles_urlpatterns()
