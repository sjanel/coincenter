#include "ssl_sha.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#include <openssl/sha.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "codec.hpp"

namespace cct::ssl {
namespace {

auto ShaDigestLen(ShaType shaType) { return static_cast<unsigned int>(shaType); }

const EVP_MD* GetEVPMD(ShaType shaType) { return shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(); }
}  // namespace

Md256 Sha256(std::string_view data) {
  static_assert(SHA256_DIGEST_LENGTH == static_cast<unsigned int>(ShaType::kSha256));

  Md256 ret(static_cast<string::size_type>(SHA256_DIGEST_LENGTH));

  SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         reinterpret_cast<unsigned char*>(ret.data()));

  return ret;
}

std::string_view GetOpenSSLVersion() { return OPENSSL_VERSION_TEXT; }

Md512 ShaBin(ShaType shaType, std::string_view data, std::string_view secret) {
  unsigned int len = ShaDigestLen(shaType);
  Md512 binData(static_cast<Md512::size_type>(len));

  HMAC(GetEVPMD(shaType), secret.data(), static_cast<int>(secret.size()),
       reinterpret_cast<const unsigned char*>(data.data()), data.size(),
       reinterpret_cast<unsigned char*>(binData.data()), &len);

  if (len != binData.size()) {
    throw exception("Unexpected result from HMAC: expected len {}, got {}", binData.size(), len);
  }
  return binData;
}

string ShaHex(ShaType shaType, std::string_view data, std::string_view secret) {
  unsigned int len = ShaDigestLen(shaType);
  unsigned char binData[EVP_MAX_MD_SIZE];

  HMAC(GetEVPMD(shaType), secret.data(), static_cast<int>(secret.size()),
       reinterpret_cast<const unsigned char*>(data.data()), data.size(), binData, &len);

  return BinToHex(std::span<const unsigned char>(binData, len));
}

namespace {
using EVPMDCTXUniquePtr = std::unique_ptr<EVP_MD_CTX, decltype([](EVP_MD_CTX* ptr) { EVP_MD_CTX_free(ptr); })>;

EVPMDCTXUniquePtr InitEVPMDCTXUniquePtr(ShaType shaType) {
  EVPMDCTXUniquePtr mdctx(EVP_MD_CTX_new());

  EVP_DigestInit_ex(mdctx.get(), GetEVPMD(shaType), nullptr);

  return mdctx;
}

string EVPBinToHex(const EVPMDCTXUniquePtr& mdctx) {
  unsigned int len = 0;
  unsigned char binData[EVP_MAX_MD_SIZE];

  EVP_DigestFinal_ex(mdctx.get(), binData, &len);

  return BinToHex(std::span<const unsigned char>(binData, len));
}
}  // namespace

string ShaDigest(ShaType shaType, std::string_view data) {
  EVPMDCTXUniquePtr mdctx = InitEVPMDCTXUniquePtr(shaType);

  EVP_DigestUpdate(mdctx.get(), data.data(), data.size());

  return EVPBinToHex(mdctx);
}

string ShaDigest(ShaType shaType, std::span<const string> data) {
  EVPMDCTXUniquePtr mdctx = InitEVPMDCTXUniquePtr(shaType);

  std::ranges::for_each(data, [&](std::string_view str) { EVP_DigestUpdate(mdctx.get(), str.data(), str.size()); });

  return EVPBinToHex(mdctx);
}

}  // namespace cct::ssl
