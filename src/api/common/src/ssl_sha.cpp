#include "ssl_sha.hpp"

#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "cct_exception.hpp"
#include "codec.hpp"

namespace cct::ssl {
namespace {

unsigned int ShaDigestLen(ShaType shaType) { return static_cast<unsigned int>(shaType); }

const EVP_MD* GetEVPMD(ShaType shaType) { return shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(); }
}  // namespace

void AppendSha256(std::string_view data, string& str) {
  static_assert(SHA256_DIGEST_LENGTH == static_cast<unsigned int>(ShaType::kSha256));

  str.resize(str.size() + static_cast<string::size_type>(SHA256_DIGEST_LENGTH));

  SHA256(
      reinterpret_cast<const unsigned char*>(data.data()), data.size(),
      reinterpret_cast<unsigned char*>(str.data() + str.size() - static_cast<string::size_type>(SHA256_DIGEST_LENGTH)));
}

std::string_view GetOpenSSLVersion() { return OPENSSL_VERSION_TEXT; }

Md ShaBin(ShaType shaType, std::string_view data, std::string_view secret) {
  unsigned int len = ShaDigestLen(shaType);
  Md binData(len, 0);

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

inline EVPMDCTXUniquePtr InitEVPMDCTXUniquePtr(ShaType shaType) {
  EVPMDCTXUniquePtr mdctx(EVP_MD_CTX_new());
  EVP_DigestInit_ex(mdctx.get(), GetEVPMD(shaType), nullptr);
  return mdctx;
}

inline string EVPBinToHex(const EVPMDCTXUniquePtr& mdctx) {
  unsigned int len;
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
