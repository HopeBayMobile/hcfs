"""
    The widgets for pdcm apps.
"""
from django.forms.widgets import TextInput, RadioInput, RadioSelect, CheckboxSelectMultiple, RadioFieldRenderer, CheckboxInput
from django.utils.safestring import mark_safe
from django.utils.encoding import force_unicode
from django.utils.html import conditional_escape
from itertools import chain
from django.forms.util import flatatt

__all__ = ['NumberInput',
           'SliderInput',
           'RadioSelect',
           'ButtonBooleanSelect',
           'CheckboxSelectMultiple',
           'IPAddressInput']


class IPAddressInput(TextInput):
    separator = u'.'
    input_number = 4

    def value_from_datadict(self, data, files, name):
        """
        Given a dictionary of data and this widget's name, returns the value
        of this widget. Returns None if it's not provided.
        """
        input_list = data.getlist(name)
        data = self.separator.join(input_list)
        if any(input_list):
            return data
        else:
            return None

    def render(self, name, value, attrs=None):
        if value is None:
            value = ''
        final_attrs = self.build_attrs(attrs, type=self.input_type, name=name)
        final_attrs['class'] = 'span05'
        final_attrs['maxlength'] = '3'

        value_list = [''] * self.input_number
        if value != '':
            value_list = value.split(self.separator)

        #create 4 input for IP address
        input_list = []
        for i in range(self.input_number):
            final_attrs['value'] = force_unicode(self._format_value(value_list[i]))
            if not value_list[i]:
                del final_attrs['value']
            input_list.append(u'<input%s />' % flatatt(final_attrs))
        render_mark = self.separator.join(input_list)

        return mark_safe(render_mark)


class NumberInput(TextInput):
    input_type = 'number'


class SliderInput(TextInput):
    input_type = 'number'

    def render(self, name, value, attrs=None):
        if value is None:
            value = ''
        final_attrs = self.build_attrs(attrs, type=self.input_type, name=name)
        if value != '':
            # Only add the 'value' attribute if a value is non-empty.
            final_attrs['value'] = force_unicode(self._format_value(value))
        output = []
        output.append(u'<div id="%s_slider" style="width:200px;margin:10px;"></div>' % name)
        output.append(u'<div id="%s_value" style="text-align:left;margin-left:100px; margin-top:-5px;">%s</div>' % (name, value))
        output.append(u'<input%s/>' % flatatt(final_attrs))
        return mark_safe(u'\n'.join(output))


class ButtonRadioInput(RadioInput):
    """An input which renders in the bootstrap button style"""

    def __unicode__(self):
        return self.render()

    def render(self, name=None, value=None, attrs=None, choices=()):
        """Rewrite the render function """
        name = name or self.name
        value = value or self.value
        attrs = attrs or self.attrs
        final_attrs = dict(type='button')
        final_attrs['class'] = 'btn'
        if self.is_checked():
            final_attrs['class'] += ' btn-primary active'
        context = {"choice_label": self.choice_label,
                   "input_tag": self.tag(),
                   "button_attrs": flatatt(final_attrs)}
        return mark_safe(u"<button%(button_attrs)s>%(input_tag)s%(choice_label)s</button>" % context)

    def tag(self):
        if 'id' in self.attrs:
            self.attrs['id'] = '%s_%s' % (self.attrs['id'], self.index)
        final_attrs = dict(self.attrs, type='radio', name=self.name, value=self.choice_value)
        final_attrs['class'] = 'hide'
        if self.is_checked():
            final_attrs['checked'] = 'checked'
        return mark_safe(u'<input%s />' % flatatt(final_attrs))


class ButtonBooleanSelectRenderer(RadioFieldRenderer):
    """Button-style boolean selector"""

    def __iter__(self):
        for i, choice in enumerate(self.choices):
            yield ButtonRadioInput(self.name, self.value, self.attrs.copy(), choice, i)

    def __getitem__(self, idx):
        choice = self.choices[idx]
        return ButtonRadioInput(self.name, self.value, self.attrs.copy(), choice, idx)

    def render(self):
        return mark_safe(u'<div id="%s" class="btn-group" data-toggle="buttons-radio">\n%s\n</div>' % (self.name, u'\n'.join([u'%s' % force_unicode(w) for w in self])))


class ButtonBooleanSelect(RadioSelect):
    renderer = ButtonBooleanSelectRenderer


class RadioFieldRenderer(RadioFieldRenderer):

    def render(self):
        return mark_safe(u'\n'.join([u'%s' % force_unicode(w) for w in self]))


class RadioSelect(RadioSelect):
    renderer = RadioFieldRenderer


class CheckboxSelectMultiple(CheckboxSelectMultiple):
    """CheckboxSelectMultiple without <ul> and <li>."""
    def render(self, name, value, attrs=None, choices=()):
        if value is None:
            value = []
        has_id = attrs and 'id' in attrs
        final_attrs = self.build_attrs(attrs, name=name)
        output = []
        # Normalize to strings
        str_values = set([force_unicode(v) for v in value])
        for i, (option_value, option_label) in enumerate(chain(self.choices, choices)):
            # If an ID attribute was given, add a numeric index as a suffix,
            # so that the checkboxes don't all have the same ID attribute.
            if has_id:
                final_attrs = dict(final_attrs, id='%s_%s' % (attrs['id'], i))
                label_for = u' for="%s"' % final_attrs['id']
            else:
                label_for = ''

            cb = CheckboxInput(final_attrs, check_test=lambda value: value in str_values)
            option_value = force_unicode(option_value)
            rendered_cb = cb.render(name, option_value)
            option_label = conditional_escape(force_unicode(option_label))
            output.append(u'<label%s>%s %s</label>' % (label_for, rendered_cb, option_label))
        return mark_safe(u'\n'.join(output))
