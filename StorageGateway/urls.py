from django.conf.urls.defaults import patterns, include, url
from django.contrib.staticfiles.urls import staticfiles_urlpatterns

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()

urlpatterns = patterns('',
    # Examples:
    # url(r'^$', 'StorageGateway.views.home', name='home'),
    # url(r'^StorageGateway/', include('StorageGateway.foo.urls')),

    # Uncomment the admin/doc line below to enable admin documentation:
    # url(r'^admin/doc/', include('django.contrib.admindocs.urls')),

    # Uncomment the next line to enable the admin:
    # url(r'^admin/', include(admin.site.urls)),
    url(r'^$', 'views.home', name='home'),
    url(r'^login$', 'views.login', name='login'),
    url(r'^logout$', 'views.logout', name='logout'),
    url(r'^wizard/', include('wizard.urls', namespace="wizard")),
    url(r'^dashboard/', include('dashboard.urls', namespace="dashboard")),
    url(r'^test_form/$', "test_view.test_form"),
) + staticfiles_urlpatterns()
