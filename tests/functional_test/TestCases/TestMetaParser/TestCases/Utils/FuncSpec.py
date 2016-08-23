import config


def tuple_yield(values, specs):
    assert len(values) == len(specs), "Values length doesn't match specs"
    for i, spec in enumerate(specs):
        yield values[i], spec  # yield for out function (match)  as args


def list_yield(values, specs):
    for i, value in enumerate(values):
        # [int]         <-> [1,2,4,6]
        # [str, int]    <-> ["sdasdas", 1, 3]
        spec = specs[i] if i < len(specs) else specs[len(specs) - 1]
        yield value, spec  # yield for out function (match)  as args


def dict_yield(value, spec):
    for key, value_spec in spec.items():
        assert key in value, "Value doesn't contain spec defined key "
        yield value[key], value_spec  # yield for out function (match)  as args


class FuncSpec(object):
    # TODO: unsupport optional argument, you should create seperated FuncSpec
    # for every possible input with optional argument
    type_handler_pair = {int: None, long: None, float: None, str: None, bool: None,
                         unicode: None, tuple: tuple_yield, list: list_yield, dict: dict_yield}

    def __init__(self, in_spec_str, out_spec_str, err_spec_str=[]):
        # spec string : [int, ...] or [[(str, int)], (str, str), ...]
        # dictionary must specify "key" value, but "value" gives type ex: {2:str,
        # "dsadasdas":int}
        self.logger = config.get_logger().getChild(__name__)
        self.logger.debug(
            "__init__" + repr((in_spec_str, out_spec_str, err_spec_str)))
        assert isinstance(in_spec_str, list), "Spec string should be list"
        assert isinstance(out_spec_str, list), "Spec string should be list"
        assert isinstance(err_spec_str, list), "Spec string should be list"
        self.input_specs = in_spec_str
        self.output_specs = out_spec_str
        self.err_specs = err_spec_str

    def check_onNormal(self, inputs, outputs):
        try:
            self.logger.debug("check_onNormal" + repr((inputs, outputs)))
            self.match_all(inputs, self.input_specs)
            self.match_all(outputs, self.output_specs)
            return True, ""
        except AssertionError as ae:
            ae.args += repr((inputs, outputs))
            return False, ae.args

    def check_onErr(self, inputs, outputs):
        try:
            self.logger.debug("check_onErr" + repr((inputs, outputs)))
            self.match_all(inputs, self.input_specs)
            self.match_all(outputs, self.err_specs)
            return True, ""
        except AssertionError as ae:
            ae.args += repr((inputs, outputs))
            return False, ae.args

    def match_all(self, values, specs):
        self.logger.debug("match_all" + repr((values, specs)))
        assert isinstance(values, list), "Need to put values to list"
        assert len(values) == len(specs), "Values length doesn't match specs"
        for i, spec in enumerate(specs):
            self.match(values[i], spec)

    def match(self, value, spec):
        self.logger.debug("check_spec" + repr((value, spec)))
        def_type = get_spec_type(spec)
        assert isinstance(value, def_type), "Value doesn't match with spec"
        assert def_type in self.type_handler_pair, "Unknown type in defined spec"
        detail_yield = self.type_handler_pair[def_type]
        if detail_yield:
            for sub_val, sub_spec in detail_yield(value, spec):
                self.match(sub_val, sub_spec)


def get_spec_type(spec):
    return spec if isinstance(spec, type) else spec.__class__

if __name__ == "__main__":
    spec = FuncSpec([str], [[(int, str)], int])
    print spec.check_onNormal(["s"], [[(2222, "asd"), (123, "zxc")], 123])
    print spec.check_onErr(["sasdasd"], [212])
