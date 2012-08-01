from django.conf.urls import patterns, include, url

urlpatterns = patterns("",
    url(r"^accounts/$", "swift_account.views.index")),
    url(r"^accounts/t$", "swift_account.views.server_time")),
)

urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
