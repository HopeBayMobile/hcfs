import json

from django import forms
from django.forms.forms import BoundField, DeclarativeFieldsMetaclass
from django.template.loader import render_to_string

from widgets import *


__all__ = ['get_config_form', 'RenderFormMixinClass']


class BoundField(BoundField):
    # TODO: Move the two property into the Form class below.
    def label_tag(self):
        return super(BoundField, self).label_tag(attrs={'class': 'control-label'})

    def _is_required(self):
        return self.field.required
    is_required = property(_is_required)


class RenderFormMixinClass(object):
    render_template_name = "forms/_bootstrap_form.html"

    def __iter__(self):
        for name in self.fields:
            yield self[name]

    def __getitem__(self, name):
        "Returns a BoundField with the given name."
        try:
            field = self.fields[name]
        except KeyError:
            raise KeyError('Key %r not found in Form' % name)
        return BoundField(self, field, name)

    def render_items(self):
        # TODO: render each fields and return a list.
        pass

    @property
    def name(self):
        return self.__class__.__name__

    def render(self):
        return render_to_string(self.render_template_name, {"form": self})

    def fieldset_list(self):
        group_list = []
        for legend, items in self.fieldset:
            fields_list = []
            for i in range(len(items)):
                name = items[i]
                fields_list.append(self[name])

            group_list.append((legend, fields_list))

        return group_list


class ConfigFormMixinClass(RenderFormMixinClass):

    def required_fields(self):
        return [field for field in self if field.is_required]

    def optional_fields(self):
        return [field for field in self if not field.is_required]


def get_choice_value(value):
    return tuple([(o, o) for o in value])


def get_form_input(field):

    boolean_choices = ((True, "True"), (False, "False"))

    input_type = field.get('type')
    kwargs = {
        "label": field.get('description'),
        "initial": field.get("default", None),
        "required": field.get('required'),
    }
    widget_attrs = {}
    if input_type in ["text", "textarea", "pass"]:
        kwargs.update({"max_length": field.get('max'),
                       "min_length": field.get('min')})
    elif input_type in ['number', 'slider']:
        widget_attrs.update({"max": field.get('max'),
                             "min": field.get('min'),
                             "step": field.get('step')})
    try:
        if input_type == "text":
            field = forms.CharField(**kwargs)
        if input_type == "textarea":
            field = forms.CharField(widget=forms.Textarea, **kwargs)
        elif input_type == "pass":
            field = forms.CharField(widget=forms.PasswordInput, **kwargs)
        elif input_type == "bool":
            field = forms.BooleanField(widget=ButtonBooleanSelect(choices=boolean_choices),
                                       **kwargs)
        elif input_type in ["slider", "number"]:
            field = forms.IntegerField(widget=SliderInput(attrs=widget_attrs), **kwargs)
        elif input_type == "dropdown":
            field = forms.ChoiceField(choices=get_choice_value(field.get('value')), **kwargs)
        elif input_type == "radio":
            field = forms.ChoiceField(choices=get_choice_value(field.get('value')),
                                      widget=RadioSelect, **kwargs)
        elif input_type == "checkbox":
            field = forms.MultipleChoiceField(choices=get_choice_value(field.get('value')),
                                              widget=CheckboxSelectMultiple,
                                              **kwargs)
    except:
        import traceback
        traceback.print_exc()
        raise NameError("Error Rendered")
    return field


class UIConfigFormMeta(DeclarativeFieldsMetaclass):

    def __new__(mcs, name, bases, attrs):

        user_config = attrs.get('user_config', None)

        items = [(label, field, field['order'])for label, field in user_config.iteritems()]
        sorted_items = sorted(items, key=lambda item: item[2])

        for label, field, order in sorted_items:
            attrs[label] = get_form_input(field)

        return super(UIConfigFormMeta, mcs).__new__(mcs, name, bases, attrs)


def get_config_form(config, form_name="unnamed_form"):

    if isinstance(config, str):
        config = json.loads(config)
    if not isinstance(config, dict):
        raise NameError('Metadata error')

    return UIConfigFormMeta(form_name,
                            (ConfigFormMixinClass, forms.Form),
                            {"user_config": config})
