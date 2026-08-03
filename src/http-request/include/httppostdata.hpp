#pragma once

#include "flatkeyvaluestring.hpp"

namespace cct {
using HttpPostData = FlatKeyValueString<'&', '='>;
}