#pragma once
#include <string>
#include <vector>
#include "core/effect_model.h"

std::string extract_template_chain(const std::string& alias);
std::vector<ParsedEffect> parse_effect_chain(const std::string& chain);