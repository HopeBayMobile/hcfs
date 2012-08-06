from django.conf.urls import patterns, include, url

urlpatterns = patterns('swift_account.views',
    url(r'^$', "index", name="list_accounts"), #with account disable modal
    url(r'^t$', 'server_time'),
    url(r'^(?P<id>\w+)/disable$', "disable_account", name="disable_account"),
    url(r'^(?P<id>\w+)/enable$', "enable_account", name="enable_account"),
    url(r'^new$', "new_account", name="new_account"),
    url(r'^new/confirm$', "new_account_confirm", name="confirm_account"),
    url(r'^(?P<id>\w+)/update$', "update_account", name="update_account"),
    url(r'^(?P<id>\w+)/delete$', "delete_account", name="delete_account"),
    url(r'^(?P<id>\w+)/edit$', "edit_account", name="edit_account"), #modal: get passwd, reset passwd, disable user
    url(r'^password$', "get_password", name="get_password"),
    url(r'^password/reset$', "reset_password", name="reset_password"),
    url(r'^user/(?P<id>\w+)/disable$', "disable_user", name="disable_account"),
    url(r'^user/(?P<id>\w+)/enable$', "enable_account", name="enable_account"),
    url(r'^user/new$', "new_user", name="new_user"),
    url(r'^user/new/confirm$', "new_user_confirm", name="confirm_user"), #del user
)