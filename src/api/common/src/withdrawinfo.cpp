#include "withdrawinfo.hpp"

#include "cct_exception.hpp"

namespace cct {
namespace api {
InitiatedWithdrawInfo::InitiatedWithdrawInfo(const Wallet &receivingWallet, WithdrawIdView withdrawId,
                                             MonetaryAmount grossEmittedAmount)
    : _receivingWallet(receivingWallet),
      _withdrawIdOrMsgIfNotInitiated(withdrawId),
      _initiatedTime(Clock::now()),
      _grossEmittedAmount(grossEmittedAmount) {}

InitiatedWithdrawInfo::InitiatedWithdrawInfo(Wallet &&receivingWallet, WithdrawIdView withdrawId,
                                             MonetaryAmount grossEmittedAmount)
    : _receivingWallet(std::move(receivingWallet)),
      _withdrawIdOrMsgIfNotInitiated(withdrawId),
      _initiatedTime(Clock::now()),
      _grossEmittedAmount(grossEmittedAmount) {}
}  // namespace api

WithdrawInfo::WithdrawInfo(const api::InitiatedWithdrawInfo &initiatedWithdrawInfo,
                           const api::SentWithdrawInfo &sentWithdrawInfo)
    : _receivingWallet(initiatedWithdrawInfo.receivingWallet()),
      _withdrawIdOrMsgIfNotInitiated(initiatedWithdrawInfo.withdrawId()),
      _initiatedTime(initiatedWithdrawInfo.initiatedTime()),
      _receivedTime(Clock::now()),
      _netEmittedAmount(sentWithdrawInfo.netEmittedAmount()) {}

const WithdrawId &WithdrawInfo::withdrawId() const {
  if (!hasBeenInitiated()) {
    throw exception("Cannot retrieve withdraw id of an empty withdraw");
  }
  return _withdrawIdOrMsgIfNotInitiated;
}

}  // namespace cct