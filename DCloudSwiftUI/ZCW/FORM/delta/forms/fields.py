import re

from django.forms import CharField
from django.core.validators import RegexValidator

__all__ = ['CIDRAddressField']

cidr_re = re.compile(r'^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\/([0-9]|[1-2][0-9]|3[0-2])$')
validate_cidr_address = RegexValidator(cidr_re, 'Enter a valid IPv4 address.', 'invalid')


class CIDRAddressField(CharField):

    default_error_messages = {
        'invalid': 'Enter a valid CIDR address.',
    }
    default_validators = [validate_cidr_address]
