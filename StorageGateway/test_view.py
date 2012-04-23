from django import forms
from django.shortcuts import render

from lib.forms import RenderFormMixinClass
from lib.forms.widgets import *


class TestForm(RenderFormMixinClass, forms.Form):

    boolean_choices = ((True, "True"), (False, "False"))

    text = forms.CharField()
    textarea = forms.CharField(widget=forms.Textarea)
    password = forms.CharField(widget=forms.PasswordInput)
    boolean = forms.BooleanField(widget=ButtonBooleanSelect(choices=boolean_choices))
    dropdown = forms.ChoiceField(choices=boolean_choices)
    radio = forms.ChoiceField(choices=boolean_choices, widget=RadioSelect)
    checkbox = forms.MultipleChoiceField(choices=boolean_choices,
                                         widget=CheckboxSelectMultiple)


def test_form(req):
    return render(req, 'form.html', {'form': TestForm()})
