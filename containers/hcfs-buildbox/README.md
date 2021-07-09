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
# docker-hcfs-test-slave

Dockerfile for generate jenkins slave for unit test ENV

## Build

### Requiremenets

1. lxc-docker 0.10+

``` sh
$ docker build -t docker:5000/docker-hcfs-test-slave .
```

## Integrate with jenkins

### Requiremenets

1. jenkins 1.557+
2. [Docker Plugin](https://wiki.jenkins-ci.org/display/JENKINS/Docker+Plugin)

### How to use

1. Follow the setup guild in **Docker Plugin** page.
2. [Intégration continue avec Jenkins et Docker](http://philpep.org/blog/integration-continue-avec-jenkins-et-docker)