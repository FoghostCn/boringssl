/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <string>
#include <vector>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"


namespace bssl {

bool ParseKeyValueArguments(std::map<std::string, std::string> *out_args,
                            const std::vector<std::string> &args,
                            const struct argument *templates) {
  out_args->clear();

  for (size_t i = 0; i < args.size(); i++) {
    const std::string &arg = args[i];
    const struct argument *templ = nullptr;
    for (size_t j = 0; templates[j].name[0] != 0; j++) {
      if (strcmp(arg.c_str(), templates[j].name) == 0) {
        templ = &templates[j];
        break;
      }
    }

    if (templ == nullptr) {
      fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
      return false;
    }

    if (out_args->find(arg) != out_args->end()) {
      fprintf(stderr, "Duplicate argument: %s\n", arg.c_str());
      return false;
    }

    if (templ->type == kBooleanArgument) {
      (*out_args)[arg] = "";
    } else {
      if (i + 1 >= args.size()) {
        fprintf(stderr, "Missing argument for option: %s\n", arg.c_str());
        return false;
      }
      (*out_args)[arg] = args[++i];
    }
  }

  for (size_t j = 0; templates[j].name[0] != 0; j++) {
    const struct argument *templ = &templates[j];
    if (templ->type == kRequiredArgument &&
        out_args->find(templ->name) == out_args->end()) {
      fprintf(stderr, "Missing value for required argument: %s\n", templ->name);
      return false;
    }
  }

  return true;
}

void PrintUsage(const struct argument *templates) {
  for (size_t i = 0; templates[i].name[0] != 0; i++) {
    const struct argument *templ = &templates[i];
    fprintf(stderr, "%s\t%s\n", templ->name, templ->description);
  }
}

bool GetUnsigned(unsigned *out, const std::string &arg_name,
                 unsigned default_value,
                 const std::map<std::string, std::string> &args) {
  const auto &it = args.find(arg_name);
  if (it == args.end()) {
    *out = default_value;
    return true;
  }

  const std::string &value = it->second;
  if (value.empty()) {
    return false;
  }

  char *endptr;
  unsigned long int num = strtoul(value.c_str(), &endptr, 10);
  if (*endptr ||
      num > UINT_MAX) {
    return false;
  }

  *out = num;
  return true;
}

}  // namespace bssl
