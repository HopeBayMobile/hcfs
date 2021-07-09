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
#<cldoc:Return code>

Return code definition


Return code definition -

>0x{production} {itemdict} {actiondict} {resourcedict} {statusdict}

- production: "20"
- itemdict: {itemdict}
- actiondict: {actiondict}
- resourcedict: {resourcedict}
- statusdict: {statusdict}

Example return code -
>0x2001010101 means "Set config was successful."

Fields -

|{itemdict}|{actiondict}|{resourcedict}|{statusdict}|
|----------|:-----------|:-------------|:-----------|
|"API":"01"|"set":"01"|"config":"01"|"successful":"001"|
|          |"get":"02"|"property":"02"|Linux System Errors:001 - 07C|
|          |"pin":"03"|"file":"03"||
|          |"unpin":"04"|"app":"04"||
|          |"query":"05"|"stat":"05"||
