/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Widget is a very simple class used for demonstrating the use of gtest. It
// simply stores two values a string and an integer, which are returned via
// public accessors in multiple forms.

#import <string>

class Widget {
 public:
  Widget(int number, const std::string& name);
  ~Widget();

  // Public accessors to number data
  float GetFloatValue() const;
  int GetIntValue() const;

  // Public accessors to the string data
  std::string GetStringValue() const;
  void GetCharPtrValue(char* buffer, size_t max_size) const;

 private:
  // Data members
  float number_;
  std::string name_;
};
