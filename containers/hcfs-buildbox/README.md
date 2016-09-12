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