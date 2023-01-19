#include "withdrawinfo.hpp"

#include "cct_exception.hpp"

namespace cct {
namespace api {
InitiatedWithdrawInfo::InitiatedWithdrawInfo(Wallet receivingWallet, std::string_view withdrawId,
                                             MonetaryAmount grossEmittedAmount, TimePoint initiatedTime)
    : _receivingWallet(std::move(receivingWallet)),
      _withdrawIdOrMsgIfNotInitiated(withdrawId),
      _initiatedTime(initiatedTime),
      _grossEmittedAmount(grossEmittedAmount) {}
}  // namespace api

WithdrawInfo::WithdrawInfo(const api::InitiatedWithdrawInfo &initiatedWithdrawInfo, MonetaryAmount receivedAmount,
                           TimePoint receivedTime)
    : _initiatedWithdrawInfo(initiatedWithdrawInfo), _receivedTime(receivedTime), _receivedAmount(receivedAmount) {}

WithdrawInfo::WithdrawInfo(api::InitiatedWithdrawInfo &&initiatedWithdrawInfo, MonetaryAmount receivedAmount,
                           TimePoint receivedTime)
    : _initiatedWithdrawInfo(std::move(initiatedWithdrawInfo)),
      _receivedTime(receivedTime),
      _receivedAmount(receivedAmount) {}

std::string_view WithdrawInfo::withdrawId() const {
  if (!hasBeenInitiated()) {
    throw exception("Cannot retrieve withdraw id of an empty withdraw");
  }
  return _initiatedWithdrawInfo.withdrawId();
}

}  // namespace cct