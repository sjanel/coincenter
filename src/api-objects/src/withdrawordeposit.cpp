#include "withdrawordeposit.hpp"

#include "timestring.hpp"
#include "unreachable.hpp"

namespace cct {
std::string_view WithdrawOrDeposit::statusStr() const {
  switch (_status) {
    case Status::kProcessing:
      return "processing";
    case Status::kFailureOrRejected:
      return "failed";
    case Status::kSuccess:
      return "success";
    default:
      unreachable();
  }
}

string WithdrawOrDeposit::timeStr() const { return ToString(_time); }
}  // namespace cct