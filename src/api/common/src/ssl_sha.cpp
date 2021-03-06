#include "ssl_sha.hpp"

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <cassert>
#include <cstring>
#include <memory>

#include "codec.hpp"

namespace cct::ssl {
namespace {
using HMACCtxUniquePtr = std::unique_ptr<HMAC_CTX, decltype([](HMAC_CTX* ptr) { HMAC_CTX_free(ptr); })>;

unsigned int ShaDigestLen(ShaType shaType) { return (shaType == ShaType::kSha256 ? 256 : 512) / CHAR_BIT; }
}  // namespace

//------------------------------------------------------------------------------
// helper function to compute SHA256:
Sha256 ComputeSha256(std::string_view data) {
  Sha256 ret;

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, data.data(), data.length());
  SHA256_Final(reinterpret_cast<unsigned char*>(ret.data()), &ctx);

  return ret;
}

string ShaBin(ShaType shaType, std::string_view data, std::string_view secret) {
  HMACCtxUniquePtr ctx(HMAC_CTX_new());

  HMAC_Init_ex(ctx.get(), secret.data(), static_cast<int>(secret.size()),
               shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(), nullptr);
  HMAC_Update(ctx.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size());

  unsigned int len = ShaDigestLen(shaType);
  string binData(len, 0);
  HMAC_Final(ctx.get(), reinterpret_cast<unsigned char*>(binData.data()), &len);
  assert(len == binData.size());
  return binData;
}

string ShaHex(ShaType shaType, std::string_view data, std::string_view secret) {
  HMACCtxUniquePtr ctx(HMAC_CTX_new());

  HMAC_Init_ex(ctx.get(), secret.data(), static_cast<int>(secret.size()),
               shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512(), nullptr);
  HMAC_Update(ctx.get(), reinterpret_cast<const unsigned char*>(data.data()), data.size());

  unsigned int len = ShaDigestLen(shaType);
  unsigned char binData[EVP_MAX_MD_SIZE];
  HMAC_Final(ctx.get(), binData, &len);
  return BinToHex(std::span<const unsigned char>(binData, len));
}

namespace {
using EVPMDCTXUniquePtr = std::unique_ptr<EVP_MD_CTX, decltype([](EVP_MD_CTX* ptr) { EVP_MD_CTX_free(ptr); })>;

inline EVPMDCTXUniquePtr InitEVPMDCTXUniquePtr(ShaType shaType) {
  const EVP_MD* md = shaType == ShaType::kSha256 ? EVP_sha256() : EVP_sha512();
  EVPMDCTXUniquePtr mdctx(EVP_MD_CTX_new());
  EVP_DigestInit_ex(mdctx.get(), md, nullptr);
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
  EVP_DigestUpdate(mdctx.get(), data.data(), data.length());
  return EVPBinToHex(mdctx);
}

string ShaDigest(ShaType shaType, std::span<const string> data) {
  EVPMDCTXUniquePtr mdctx = InitEVPMDCTXUniquePtr(shaType);
  for (std::string_view s : data) {
    EVP_DigestUpdate(mdctx.get(), s.data(), s.length());
  }
  return EVPBinToHex(mdctx);
}

}  // namespace cct::ssl
