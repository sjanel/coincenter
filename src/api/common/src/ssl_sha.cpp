#include "ssl_sha.hpp"

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <cassert>
#include <cstring>
#include <memory>

#include "cct_codec.hpp"

namespace cct {
namespace ssl {
namespace {
using HMACCtxUniquePtr = std::unique_ptr<HMAC_CTX, decltype([](HMAC_CTX* ptr) { HMAC_CTX_free(ptr); })>;

unsigned int ShaDigestLen(ShaType shaType) { return (shaType == ShaType::kSha256 ? 256 : 512) / CHAR_BIT; }
}  // namespace

//------------------------------------------------------------------------------
// helper function to compute SHA256:
Sha256 ComputeSha256(const std::string& data) {
  Sha256 ret;

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, data.c_str(), data.length());
  SHA256_Final(reinterpret_cast<unsigned char*>(ret.data()), &ctx);

  return ret;
}

std::string ShaBin(ShaType shaType, const std::string& data, const char* secret) {
  HMACCtxUniquePtr ctx(HMAC_CTX_new());

  HMAC_Init_ex(ctx.get(), secret, static_cast<int>(strlen(secret)),
               shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(), nullptr);
  HMAC_Update(ctx.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size());

  unsigned int len = ShaDigestLen(shaType);
  std::string binData(len, 0);
  HMAC_Final(ctx.get(), reinterpret_cast<unsigned char*>(binData.data()), &len);
  assert(len == binData.size());
  return binData;
}

std::string ShaHex(ShaType shaType, const std::string& data, const char* secret) {
  HMACCtxUniquePtr ctx(HMAC_CTX_new());

  HMAC_Init_ex(ctx.get(), secret, static_cast<int>(strlen(secret)),
               shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(), nullptr);
  HMAC_Update(ctx.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size());

  unsigned int len = ShaDigestLen(shaType);
  unsigned char binData[EVP_MAX_MD_SIZE];
  HMAC_Final(ctx.get(), binData, &len);
  return BinToHex(binData, len);
}

std::string ShaDigest(ShaType shaType, std::span<const std::string> data) {
  const EVP_MD* md = shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512();

  using EVPMDCTXUniquePtr = std::unique_ptr<EVP_MD_CTX, decltype([](EVP_MD_CTX* ptr) { EVP_MD_CTX_free(ptr); })>;

  EVPMDCTXUniquePtr mdctx(EVP_MD_CTX_new());
  EVP_DigestInit_ex(mdctx.get(), md, nullptr);
  for (const std::string& s : data) {
    EVP_DigestUpdate(mdctx.get(), s.c_str(), s.length());
  }
  unsigned int len;
  unsigned char binData[EVP_MAX_MD_SIZE];
  EVP_DigestFinal_ex(mdctx.get(), binData, &len);
  return BinToHex(binData, len);
}

}  // namespace ssl
}  // namespace cct
