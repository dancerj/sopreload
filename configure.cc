/*
    Copyright 2023 Google LLC

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

*/
#include "configure.h"

int main() {
  NinjaBuilder::Config c;
  c.cxxflags = "-O2 -g --std=c++20 -Wall -Werror -I.";
  c.ldflags = "-pthread -latomic ";

  NinjaBuilder n(c);

  n.CompileLink("sopreload", {"elfphdr", "sopreload"});
  n.CompileLink("benchmark", {"benchmark"});
  n.CompileLinkRunTest("preload_only", {"elfphdr", "preload_only"});
  n.CompileLinkRunTest("scoped_timer_test", {"scoped_timer_test"});
  n.CompileLink("hello", {"hello"});
  n.RunTestScript("elfloader_integration_test.sh",
                  {"out/sopreload", "out/hello"});
}
