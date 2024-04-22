#include "withdrawinfo.hpp"

#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "wallet.hpp"

namespace cct {
namespace api {
InitiatedWithdrawInfo::InitiatedWithdrawInfo(Wallet receivingWallet, std::string_view withdrawId,
                                             MonetaryAmount grossEmittedAmount, TimePoint initiatedTime)
    : _receivingWallet(std::move(receivingWallet)),
      _withdrawIdOrMsgIfNotInitiated(withdrawId),
      _grossEmittedAmount(grossEmittedAmount),
      _initiatedTime(initiatedTime) {}
}  // namespace api

DeliveredWithdrawInfo::DeliveredWithdrawInfo(api::InitiatedWithdrawInfo &&initiatedWithdrawInfo,
                                             api::ReceivedWithdrawInfo &&receivedWithdrawInfo)
    : _initiatedWithdrawInfo(std::move(initiatedWithdrawInfo)),
      _receivedWithdrawInfo(std::move(receivedWithdrawInfo)) {}

std::string_view DeliveredWithdrawInfo::withdrawId() const {
  if (!hasBeenInitiated()) {
    throw exception("Cannot retrieve withdraw id of an empty withdraw");
  }
  return _initiatedWithdrawInfo.withdrawId();
}

}  // namespace cct