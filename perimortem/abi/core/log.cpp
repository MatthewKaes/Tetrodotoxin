// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include <stdio.h>

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/null_terminated.hpp"
#include "perimortem/core/perimortem.hpp"

using namespace Perimortem::Core;

extern "C" void print(View::Bytes data) {
  fflush(stdout);
  View::Bytes debug_text = "[DEBUG] "_view;
  View::Bytes end_line = "\n"_view;
  fwrite(debug_text.get_data(), 1, debug_text.get_size(), stdout);
  fwrite(data.get_data(), 1, data.get_size(), stdout);
  fwrite(end_line.get_data(), 1, end_line.get_size(), stdout);
}
