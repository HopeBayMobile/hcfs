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

// Widget is a very simple class used for demonstrating the use of gtest

#include "widget.h"

Widget::Widget(int number, const std::string& name)
    : number_(number),
      name_(name) {}

Widget::~Widget() {}

float Widget::GetFloatValue() const {
  return number_;
}

int Widget::GetIntValue() const {
  return static_cast<int>(number_);
}

std::string Widget::GetStringValue() const {
  return name_;
}

void Widget::GetCharPtrValue(char* buffer, size_t max_size) const {
  // Copy the char* representation of name_ into buffer, up to max_size.
  strncpy(buffer, name_.c_str(), max_size-1);
  buffer[max_size-1] = '\0';
  return;
}
