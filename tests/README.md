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
HCFS Test Suite
===============================

HCFS uses Jenkins CI to automate test and run tests in docker container to ensure the consistency of test environment.

# File Structure #

## `docker_test.sh` ##
    By executing it, developers can run CI tests at localhost. By running test image in a docker container, it will not affecting localhost's environment too much. test.sh will first install docker then run 2 images -- test slave image and swift all-in-one image -- to perform tests in containers.

## `docker_scrips/*` ##
    All tests in tests will be invove from tests/docker_scrips/*

## `docker_test_slave/*` ##
    Use to build docker image of test slave. All test requirement will added into this image to speed up test preparing process.
