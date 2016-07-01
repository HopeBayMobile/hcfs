import logging

import VarMgt

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(VarMgt.get_log_level())

# {"input":[int, ...], "output":[[(str, int)], (str, str), ...]}
# dictionary must specify "key" value, but "value" gives type ex: {2:str,
# "dsadasdas":int}


def tuple_handler(val, t_spec, spec_handler_pair):
    assert len(val) == len(t_spec), "Encorrect spec need " + \
        repr(t_spec) + ", but result is " + repr(val)
    i = 0
    for spec in t_spec:
        logger.info("[tuple_handler] checke value <" +
                    str(val[i]) + ">, spec " + repr(spec))
        check_spec(val[i], spec, spec_handler_pair)
        i = i + 1

# [int] 		<-> [1,2,4,6]
# [str, int] 	<-> ["sdasdas", 1, 3]


def list_handler(val, l_spec, spec_handler_pair):
    i = 0
    for value in val:
        spec = l_spec[i] if i < len(l_spec) else l_spec[len(l_spec) - 1]
        logger.info("[list_handler] checke value <" +
                    str(value) + ">, spec " + repr(spec))
        check_spec(value, spec, spec_handler_pair)
        i = i + 1


def dict_handler(val, d_spec, spec_handler_pair):
    for key, value_spec in d_spec.items():
        logger.info("[dict_handler] checke key <" +
                    key + ">, spec " + repr(value_spec))
        assert key in val, "Value doesn't contain key " + repr(key)
        check_spec(val[key], value_spec, spec_handler_pair)

# TODO: unsupport optional argument, you should create seperated harness
# for every possible input with optional argument


class Harness(object):
    type_handler_pair = {int: None, long: None, float: None, str: None, bool: None,
                         unicode: None, tuple: tuple_handler, list: list_handler, dict: dict_handler}

    def __init__(self, spec):
        if not isinstance(spec, dict) or not ("input" in spec) or not ("output" in spec) or not isinstance(spec["input"], list) or not isinstance(spec["output"], list):
            raise Exception(
                "Encorrect form, need {'input':[int, ...], 'output':[[(str, int)], (str, str), ...]} but " + repr(spec))
        self.input_spec = spec["input"]
        for sub_spec in self.input_spec:
            assert get_type(sub_spec) in self.type_handler_pair, "Encorrect input spec " + \
                repr(sub_spec) + ", not in " + \
                repr(self.type_handler_pair.keys())
        self.output_spec = spec["output"]
        for sub_spec in self.output_spec:
            assert get_type(sub_spec) in self.type_handler_pair, "Encorrect output spec " + \
                repr(sub_spec) + ", not in " + \
                repr(self.type_handler_pair.keys())

    def expect(self, inputs, expected, result):
        check_value_list_spec(inputs, self.input_spec, self.type_handler_pair)
        check_value_list_spec(expected, self.output_spec,
                              self.type_handler_pair)
        check_value_list_spec(result, self.output_spec, self.type_handler_pair)
        assert expected == result, "Unmatch between expected " + \
            repr(expected) + " and result " + repr(result)
        return repr({"in": inputs, "out": result})

    def check(self, inputs, result):
        check_value_list_spec(inputs, self.input_spec, self.type_handler_pair)
        check_value_list_spec(result, self.output_spec, self.type_handler_pair)
        return repr({"in": inputs, "out": result})


def check_value_list_spec(val, spec, spec_handler_pair):
    assert isinstance(
        val, list), "Need to put value to list, value = <" + val + ">"
    assert len(val) == len(spec), "Encorrect value length, spec " + repr(spec) + \
        " need " + repr(len(spec)) + ", but value " + \
        repr(val) + " is " + repr(len(val))
    i = 0
    for sub_spec in spec:
        check_spec(val[i], sub_spec, spec_handler_pair)
        i = i + 1


def check_spec(val, spec, spec_handler_pair):
    assert isinstance(val, get_type(spec)), "Encorrect spec need " + \
        str(spec) + ", but result " + repr(val) + " is " + repr(type(val))
    if spec_handler_pair[get_type(spec)]:
        spec_handler_pair[get_type(spec)](val, spec, spec_handler_pair)


def get_type(spec):
    return spec if isinstance(spec, type) else spec.__class__

if __name__ == "__main__":
    spec = Harness({"input": [str], "output": [[(int, str)]]})
    print spec.expect(["s"], [[(2222, "asd"), (123, "zxc")]], [[(2222, "asd"), (123, "zxc")]])
