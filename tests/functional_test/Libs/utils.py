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

def pi_get_path(folder_name):
    '''Return the directory path under root directory of pi-tester
    '''
    root_dir = os.path.abspath(os.getcwd())
    pi_path = os.path.join(root_dir, folder_name)
    return pi_path
    
if __name__ == '__main__':
    #get_tools_dir()
    pass