/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__pkixutil_h
#define mozilla_pkix__pkixutil_h

#include "pkix/enumclass.h"
#include "pkix/pkixtypes.h"
#include "pkixder.h"
#include "prerror.h"
#include "seccomon.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

enum Result
{
  Success = 0,
  FatalError = -1,      // An error was encountered that caused path building
                        // to stop immediately. example: out-of-memory.
  RecoverableError = -2 // an error that will cause path building to continue
                        // searching for alternative paths. example: expired
                        // certificate.
};

// When returning errors, use this function instead of calling PR_SetError
// directly. This helps ensure that we always call PR_SetError when we return
// an error code. This is a useful place to set a breakpoint when a debugging
// a certificate verification failure.
inline Result
Fail(Result result, PRErrorCode errorCode)
{
  PR_ASSERT(result != Success);
  PR_SetError(errorCode, 0);
  return result;
}

inline Result
MapSECStatus(SECStatus srv)
{
  if (srv == SECSuccess) {
    return Success;
  }

  PRErrorCode error = PORT_GetError();
  switch (error) {
    case SEC_ERROR_EXTENSION_NOT_FOUND:
      return RecoverableError;

    case PR_INVALID_STATE_ERROR:
    case SEC_ERROR_LIBRARY_FAILURE:
    case SEC_ERROR_NO_MEMORY:
      return FatalError;
  }

  // TODO: PORT_Assert(false); // we haven't classified the error yet
  return RecoverableError;
}

// During path building and verification, we build a linked list of BackCerts
// from the current cert toward the end-entity certificate. The linked list
// is used to verify properties that aren't local to the current certificate
// and/or the direct link between the current certificate and its issuer,
// such as name constraints.
//
// Each BackCert contains pointers to all the given certificate's extensions
// so that we can parse the extension block once and then process the
// extensions in an order that may be different than they appear in the cert.
class BackCert
{
public:
  // IncludeCN::No means that name constraint enforcement should not consider
  // the subject CN as a possible dNSName. IncludeCN::Yes means that name
  // constraint enforcement will consider the subject CN as a possible dNSName.
  MOZILLA_PKIX_ENUM_CLASS IncludeCN { No = 0, Yes = 1 };

  // certDER and childCert must be valid for the lifetime of BackCert.
  BackCert(const SECItem& certDER, const BackCert* childCert,
           IncludeCN includeCN)
    : der(certDER)
    , childCert(childCert)
    , includeCN(includeCN)
  {
  }

  Result Init();

  const SECItem& GetDER() const { return der; }
  const der::Version GetVersion() const { return version; }
  const CERTSignedData& GetSignedData() const { return signedData; }
  const SECItem& GetIssuer() const { return issuer; }
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  const SECItem& GetValidity() const { return validity; }
  const SECItem& GetSerialNumber() const { return serialNumber; }
  const SECItem& GetSubject() const { return subject; }
  const SECItem& GetSubjectPublicKeyInfo() const
  {
    return subjectPublicKeyInfo;
  }
  const SECItem* GetAuthorityInfoAccess() const
  {
    return MaybeSECItem(authorityInfoAccess);
  }
  const SECItem* GetBasicConstraints() const
  {
    return MaybeSECItem(basicConstraints);
  }
  const SECItem* GetCertificatePolicies() const
  {
    return MaybeSECItem(certificatePolicies);
  }
  const SECItem* GetExtKeyUsage() const { return MaybeSECItem(extKeyUsage); }
  const SECItem* GetKeyUsage() const { return MaybeSECItem(keyUsage); }
  const SECItem* GetInhibitAnyPolicy() const
  {
    return MaybeSECItem(inhibitAnyPolicy);
  }
  const SECItem* GetNameConstraints() const
  {
    return MaybeSECItem(nameConstraints);
  }

private:
  const SECItem& der;

public:
  BackCert const* const childCert;
  const IncludeCN includeCN;

private:
  der::Version version;

  // When parsing certificates in BackCert::Init, we don't accept empty
  // extensions. Consequently, we don't have to store a distinction between
  // empty extensions and extensions that weren't included. However, when
  // *processing* extensions, we distinguish between whether an extension was
  // included or not based on whetehr the GetXXX function for the extension
  // returns nullptr.
  static inline const SECItem* MaybeSECItem(const SECItem& item)
  {
    return item.len > 0 ? &item : nullptr;
  }

  // Helper classes to zero-initialize these fields on construction and to
  // document that they contain non-owning pointers to the data they point
  // to.
  struct NonOwningSECItem : public SECItemStr {
    NonOwningSECItem()
    {
      data = nullptr;
      len = 0;
    }
  };
  struct NonOwningCERTSignedData : public CERTSignedDataStr {
    NonOwningCERTSignedData() { memset(this, 0, sizeof(*this)); }
  };

  NonOwningCERTSignedData signedData;
  NonOwningSECItem issuer;
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  NonOwningSECItem validity;
  NonOwningSECItem serialNumber;
  NonOwningSECItem subject;
  NonOwningSECItem subjectPublicKeyInfo;

  NonOwningSECItem authorityInfoAccess;
  NonOwningSECItem basicConstraints;
  NonOwningSECItem certificatePolicies;
  NonOwningSECItem extKeyUsage;
  NonOwningSECItem inhibitAnyPolicy;
  NonOwningSECItem keyUsage;
  NonOwningSECItem nameConstraints;
  NonOwningSECItem subjectAltName;

  der::Result RememberExtension(der::Input& extnID, const SECItem& extnValue,
                                /*out*/ bool& understood);

  BackCert(const BackCert&) /* = delete */;
  void operator=(const BackCert&); /* = delete */;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixutil_h
