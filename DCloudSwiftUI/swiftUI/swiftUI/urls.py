from django.conf import settings
from django.conf.urls import patterns, include, url
from django.conf.urls.static import static

from django.views.generic.simple import direct_to_template

from django.contrib import admin
admin.autodiscover()


urlpatterns = patterns("",
    #url(r"^$", direct_to_template, {"template": "homepage.html"}, name="home"),
    url(r"^$", include("swift_dashboard.urls", namespace="dashboard")),
    url(r"^admin/", include(admin.site.urls)),
    url(r"^account/", include("account.urls")),
    # custom apps
    url(r"^accounts/", include("swift_account.urls", namespace="accounts")),
    url(r"^monitor/", include("swift_monitor.urls", namespace="monitor")),
    url(r"^maintainance/", include("swift_maintainance.urls", namespace="maintainance")),
    url(r"^systemlog/", include("systemlog.urls", namespace="log")),
)

urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
