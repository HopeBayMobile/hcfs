import os


def listdir_full(directory, filter_func=None, args=None):
    if filter_func:
        if args:
            return [(os.path.join(directory, x), x) for x in os.listdir(directory) if filter_func(x, args)]
        else:
            return [(os.path.join(directory, x), x) for x in os.listdir(directory) if filter_func(x)]
    else:
        return [(os.path.join(directory, x), x) for x in os.listdir(directory)]


def listdir_path(directory, filter_func=None, args=None):
    path_name_pairs = listdir_full(directory, filter_func, args)
    pathes, names = zip(*path_name_pairs)
    return pathes


def negate(func):
    def new(obj, args=None):
        if args:
            return not func(obj, *args)
        else:
            return not func(obj)
    return new


def run_once(f):
    def wrapper(*args, **kwargs):
        if not wrapper.has_run:
            wrapper.has_run = True
            return f(*args, **kwargs)
    wrapper.has_run = False
    return wrapper

if __name__ == '__main__':
    notstartswith = negate(str.startswith)
    path = "/home/test/workspace/python"
    print listdir_full(path, notstartswith, ("con",))
    print listdir_full(path, str.startswith, ("con",))
    print listdir_full(path)
    print "-" * 40
    notdigit = negate(str.isdigit)
    path = "/home/test/workspace/python"
    print listdir_full(path, notdigit)
    print listdir_full(path, str.isdigit)
    print listdir_full(path)

    @run_once
    def pprint(i):
        print "times:" + str(i)

    for i in range(10):
        pprint(i)
    pprint(213123)
