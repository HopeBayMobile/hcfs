<!--
Copyright (c) 2021 HopeBayTech.

This file is part of Tera.
See https://github.com/HopeBayMobile for further info.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->
#<cldoc:Return value>

Return value definition


Return value definition -

>```json
{
    result: boolean,
    code: int,
    data: dictionary
}
```

Fields -

- result: To determine if this operation is successful or not.
- code: More details about this operation. (Code is defined in section of each API.)
- data: Additional key/value pairs returned by this operation. (Data is defined in section of each API.)

Example -

>```json
{
    result: True,
    code: 0,
    data: {
        key1: val1,
        key2: val2,
    }
}
```


