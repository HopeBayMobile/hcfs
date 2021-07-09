#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import os


def list_abspathes_filter_name(directory, filter_func):
    return [os.path.join(directory, x) for x in os.listdir(directory) if filter_func(x)]
    # return [(os.path.join(directory, x), x) for x in os.listdir(directory)]


def file_name(path):
    return path.split("/")[-1]


# filter function
def not_startswith(word):
    def filter_func(check_one):
        return not check_one.startswith(word)
    return filter_func


# filter function
def startswith(word):
    def filter_func(check_one):
        return check_one.startswith(word)
    return filter_func


# filter function
def not_digit():
    def filter_func(check_one):
        return not check_one.isdigit()
    return filter_func

if __name__ == '__main__':
    path = "/home/test/workspace/python/Utils/Adapter"
    print list_abspathes_filter_name(path, not_startswith("M"))
    from functools import partial
    print list_abspathes_filter_name(path, startswith("M"))
    print "-" * 40
    print list_abspathes_filter_name(path, not_digit())
    print list_abspathes_filter_name(path, str.isdigit)

    def filter_paths(paths, filter_func, *args):
        print args
        if len(args) == 1:
            return filter(filter_func(args[0]), paths)
        elif len(args) == 2:
            return filter(filter_func(args[0], args[1]), paths)

    a = ["asd", "dddd", "aasssssssssss", "as"]
    print filter(not_startswith("as"), a)
