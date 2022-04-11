#include "withdrawinfo.hpp"

#include "cct_exception.hpp"

namespace cct {
namespace api {
InitiatedWithdrawInfo::InitiatedWithdrawInfo(Wallet receivingWallet, WithdrawIdView withdrawId,
                                             MonetaryAmount grossEmittedAmount, TimePoint initiatedTime)
    : _receivingWallet(std::move(receivingWallet)),
      _withdrawIdOrMsgIfNotInitiated(withdrawId),
      _initiatedTime(initiatedTime),
      _grossEmittedAmount(grossEmittedAmount) {}
}  // namespace api

WithdrawInfo::WithdrawInfo(const api::InitiatedWithdrawInfo &initiatedWithdrawInfo,
                           const api::SentWithdrawInfo &sentWithdrawInfo, TimePoint receivedTime)
    : _receivingWallet(initiatedWithdrawInfo.receivingWallet()),
      _withdrawIdOrMsgIfNotInitiated(initiatedWithdrawInfo.withdrawId()),
      _initiatedTime(initiatedWithdrawInfo.initiatedTime()),
      _receivedTime(receivedTime),
      _netEmittedAmount(sentWithdrawInfo.netEmittedAmount()) {}

const WithdrawId &WithdrawInfo::withdrawId() const {
  if (!hasBeenInitiated()) {
    throw exception("Cannot retrieve withdraw id of an empty withdraw");
  }
  return _withdrawIdOrMsgIfNotInitiated;
}

}  // namespace cct