#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x -e

docker build -t docker:5000/android-buildbox .
docker push docker:5000/android-buildbox
