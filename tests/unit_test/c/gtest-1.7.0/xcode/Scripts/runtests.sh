#!/bin/bash
##
## Copyright (c) 2021 HopeBayTech.
##
## This file is part of Tera.
## See https://github.com/HopeBayMobile for further info.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

# Executes the samples and tests for the Google Test Framework.

# Help the dynamic linker find the path to the libraries.
export DYLD_FRAMEWORK_PATH=$BUILT_PRODUCTS_DIR
export DYLD_LIBRARY_PATH=$BUILT_PRODUCTS_DIR

# Create some executables.
test_executables=("$BUILT_PRODUCTS_DIR/gtest_unittest-framework"
                  "$BUILT_PRODUCTS_DIR/gtest_unittest"
                  "$BUILT_PRODUCTS_DIR/sample1_unittest-framework"
                  "$BUILT_PRODUCTS_DIR/sample1_unittest-static")

# Now execute each one in turn keeping track of how many succeeded and failed. 
succeeded=0
failed=0
failed_list=()
for test in ${test_executables[*]}; do
  "$test"
  result=$?
  if [ $result -eq 0 ]; then
    succeeded=$(( $succeeded + 1 ))
  else
    failed=$(( failed + 1 ))
    failed_list="$failed_list $test"
  fi
done

# Report the successes and failures to the console.
echo "Tests complete with $succeeded successes and $failed failures."
if [ $failed -ne 0 ]; then
  echo "The following tests failed:"
  echo $failed_list
fi
exit $failed
