#include "security/signature.h"
#include <tools/sha256.h>
#if defined (__ANDROID__)

#include "app.h"
#include <jni.h>
#include <android/log.h>

struct ApkSigChecker {
    // expectedSha256: raw 32 bytes of the SHA-256 of the signer's certificate
    // apkPath: path to the APK, readable from /proc/self/maps or passed in
    static bool verify(const std::string& apkPath, uint8_t dest[32]) {
        FILE* f = fopen(apkPath.c_str(), "rb");
        if (!f) { return false; }

        // find ZIP end-of-central-directory
        uint32_t eocdOffset = 0;
        if (!findEOCD(f, eocdOffset)) {
            fclose(f); return false;
        }

        // read central directory offset from EOCD
        uint32_t cdOffset = 0;
        fseek(f, eocdOffset + 16, SEEK_SET);
        fread(&cdOffset, 4, 1, f);

        // APK Signing Block sits immediately before the central directory
        // search backwards from cdOffset for the magic
        uint64_t sigBlockOffset = 0;
        uint64_t sigBlockSize = 0;
        if (!findSigningBlock(f, cdOffset, sigBlockOffset, sigBlockSize)) {
            fclose(f); return false;
        }

        // parse ID-value pairs inside the signing block
        // looking for ID 0x7109871a (v2) or 0xf05368c0 (v3)
        std::vector<uint8_t> block(sigBlockSize);
        fseek(f, (long)sigBlockOffset + 8, SEEK_SET); // skip size field
        fread(block.data(), 1, sigBlockSize - 24, f); // skip size+magic at end
        fclose(f);

        return parseSignerCert(block.data(), (uint32_t)(sigBlockSize - 24), dest);
    }

    // Helper: get APK path from /proc/self/cmdline + /proc/self/maps
    static std::string getApkPath() {
        // Read own package name from cmdline
        char cmdline[256]{};
        FILE* f = fopen("/proc/self/cmdline", "r");
        if (f) { fread(cmdline, 1, sizeof(cmdline)-1, f); fclose(f); }

        // Scan maps for the .apk
        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps) return {};
        char line[512];
        std::string result;
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, ".apk") && strstr(line, cmdline)) {
                // extract path
                char* path = strchr(line, '/');
                if (path) {
                    path[strcspn(path, "\n")] = 0;
                    result = path;
                    break;
                }
            }
        }
        fclose(maps);
        return result;
    }

    // ── ZIP EOCD ──────────────────────────────────────────────────────────
    static bool findEOCD(FILE* f, uint32_t& outOffset) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        // EOCD is at least 22 bytes, comment up to 65535
        long searchStart = size - 22 - 65535;
        if (searchStart < 0) searchStart = 0;

        std::vector<uint8_t> buf(size - searchStart);
        fseek(f, searchStart, SEEK_SET);
        fread(buf.data(), 1, buf.size(), f);

        for (long i = (long)buf.size() - 22; i >= 0; i--) {
            if (buf[i]==0x50 && buf[i+1]==0x4b && buf[i+2]==0x05 && buf[i+3]==0x06) {
                outOffset = (uint32_t)(searchStart + i);
                return true;
            }
        }
        return false;
    }

    // ── APK Signing Block ─────────────────────────────────────────────────
    // magic: "APK Sig Block 42" = 16 bytes at the end of the block
    static constexpr uint8_t kMagic[16] = {
            'A','P','K',' ','S','i','g',' ','B','l','o','c','k',' ','4','2'
    };

    static bool findSigningBlock(FILE* f, uint32_t cdOffset,
                                 uint64_t& outOffset, uint64_t& outSize) {
        if (cdOffset < 32) return false;

        // Read potential magic + size footer (24 bytes before CD)
        uint8_t footer[24];
        fseek(f, (long)cdOffset - 24, SEEK_SET);
        fread(footer, 1, 24, f);

        if (memcmp(footer + 8, kMagic, 16) != 0) return false;

        uint64_t blockSize;
        memcpy(&blockSize, footer, 8);
        if (blockSize < 32 || blockSize > cdOffset) return false;

        // block starts at cdOffset - blockSize - 8
        outOffset = cdOffset - blockSize - 8;
        outSize   = blockSize + 8; // include the leading size field
        return true;
    }

    // ── Parse ID-value pairs, find v2/v3 signer cert, SHA256 it ──────────
    static bool parseSignerCert(const uint8_t* data, uint32_t size,
                                uint8_t dest[32]) {
        uint32_t pos = 0;

        while (pos + 8 <= size) {
            uint64_t pairLen;
            memcpy(&pairLen, data + pos, 8);
            pos += 8;
            if (pairLen < 4 || pos + pairLen > size) break;

            uint32_t id;
            memcpy(&id, data + pos, 4);

            if (id == 0x7109871au || id == 0xf05368c0u) {
                // found v2 or v3 block — parse signers
                const uint8_t* block = data + pos + 4;
                uint32_t blockLen = (uint32_t)pairLen - 4;

                if (extractAndHashCert(block, blockLen, dest))
                    return true;
            }
            pos += (uint32_t)pairLen;
        }
        return false;
    }

    // v2/v3 structure:
    // signers sequence (length-prefixed)
    //   signer (length-prefixed)
    //     signed data (length-prefixed)
    //       certificates sequence (length-prefixed)
    //         certificate (length-prefixed) ← raw DER X.509
    static bool extractAndHashCert(const uint8_t* data, uint32_t size, uint8_t dest[32]) {
        if (size < 4) return false;

        uint32_t signersLen;
        memcpy(&signersLen, data, 4);
        uint32_t pos = 4;
        if (pos + signersLen > size) return false;

        while (pos + 4 <= 4 + signersLen) {
            uint32_t signerLen;
            memcpy(&signerLen, data + pos, 4);
            pos += 4;
            if (pos + signerLen > size) break;
            const uint8_t* signer = data + pos;

            if (signerLen < 4) { pos += signerLen; continue; }
            uint32_t sdLen;
            memcpy(&sdLen, signer, 4);
            if (sdLen + 4 > signerLen) { pos += signerLen; continue; }
            const uint8_t* sd = signer + 4;

            if (sdLen < 4) { pos += signerLen; continue; }
            uint32_t digestsLen;
            memcpy(&digestsLen, sd, 4);
            uint32_t sdPos = 4 + digestsLen;

            if (sdPos + 4 > sdLen) { pos += signerLen; continue; }
            uint32_t certsLen;
            memcpy(&certsLen, sd + sdPos, 4);
            sdPos += 4;

            uint32_t certsEnd = sdPos + certsLen;
            if (sdPos + 4 <= certsEnd && sdPos + 4 <= sdLen) {
                uint32_t certLen;
                memcpy(&certLen, sd + sdPos, 4);
                sdPos += 4;
                if (sdPos + certLen <= sdLen) {
                    tsl::sha256::sha256(sd + sdPos, certLen, dest);
                    return true;
                }
            }
            pos += signerLen;
        }
        return false;
    }

};

bool verify_signature_universal(JNIEnv* env, uint8_t* dest) {


    // 2. Get the Context via your Singleton
    // We use the cached appClass directly
    std::string instanceSig = std::string("()L") + tsl::android::AppClassPath + ";";
    jmethodID getInstanceMid = env->GetStaticMethodID(tsl::android::appclass, "getInstance", instanceSig.c_str());
    if (env->ExceptionCheck() || !getInstanceMid) { env->ExceptionClear(); return false; }

    jobject context = env->CallStaticObjectMethod(tsl::android::appclass, getInstanceMid);
    if (!context) return false;

    // 3. Get SDK Version
    jclass buildClass = env->FindClass("android/os/Build$VERSION");
    if (!buildClass) return false;
    jfieldID sdkIntFid = env->GetStaticFieldID(buildClass, "SDK_INT", "I");
    if (!sdkIntFid) return false;
    jint sdkInt = env->GetStaticIntField(buildClass, sdkIntFid);

    // 4. Access PackageManager via Context
    jclass ctxCls = env->GetObjectClass(context);
    jmethodID getPkgNameMid = env->GetMethodID(ctxCls, "getPackageName", "()Ljava/lang/String;");
    jmethodID getPMMid = env->GetMethodID(ctxCls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!getPkgNameMid || !getPMMid) return false;

    jstring pkgName = (jstring)env->CallObjectMethod(context, getPkgNameMid);
    jobject pm = env->CallObjectMethod(context, getPMMid);
    if (!pkgName || !pm) return false;

    // 5. Select Flags (v2/v3 support)
    int flags = (sdkInt >= 28) ? 0x08000000 : 0x00000040; // GET_SIGNING_CERTIFICATES : GET_SIGNATURES
    jclass pmCls = env->GetObjectClass(pm);
    jmethodID getPkgInfoMid = env->GetMethodID(pmCls, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    if (!getPkgInfoMid) return false;
    jobject pkgInfo = env->CallObjectMethod(pm, getPkgInfoMid, pkgName, flags);

    if (env->ExceptionCheck() || !pkgInfo) { env->ExceptionClear(); return false; }

    jbyteArray certBytes = nullptr;
    jclass piCls = env->GetObjectClass(pkgInfo);

    // 6. Navigate to Certificate Data
    if (sdkInt >= 28) {
        // API 28+: Use signingInfo
        jfieldID siFid = env->GetFieldID(piCls, "signingInfo", "Landroid/content/pm/SigningInfo;");
        if (!siFid) return false;
        jobject si = env->GetObjectField(pkgInfo, siFid);
        if (!si) return false;
        jmethodID getSigsMid = env->GetMethodID(env->GetObjectClass(si), "getApkContentsSigners", "()[Landroid/content/pm/Signature;");
        if (!getSigsMid) return false;
        auto sigs = (jobjectArray)env->CallObjectMethod(si, getSigsMid);
        if (!sigs || env->GetArrayLength(sigs) == 0) return false;
        jobject firstSig = env->GetObjectArrayElement(sigs, 0);
        jmethodID toByteMid = env->GetMethodID(env->GetObjectClass(firstSig), "toByteArray", "()[B");
        if (!toByteMid) return false;
        certBytes = (jbyteArray)env->CallObjectMethod(firstSig, toByteMid);
    } else {
        // API 24-27: Use signatures array
        jfieldID sigsFid = env->GetFieldID(piCls, "signatures", "[Landroid/content/pm/Signature;");
        if (!sigsFid) return false;
        auto sigs = (jobjectArray)env->GetObjectField(pkgInfo, sigsFid);
        if (!sigs || env->GetArrayLength(sigs) == 0) return false;
        jobject firstSig = env->GetObjectArrayElement(sigs, 0);
        jmethodID toByteMid = env->GetMethodID(env->GetObjectClass(firstSig), "toByteArray", "()[B");
        if (!toByteMid) return false;
        certBytes = (jbyteArray)env->CallObjectMethod(firstSig, toByteMid);
    }

    if (!certBytes) return false;

    // 7. Hash the raw DER certificate
    jbyte* raw = env->GetByteArrayElements(certBytes, nullptr);
    jsize len = env->GetArrayLength(certBytes);

    tsl::sha256::sha256((uint8_t*)raw, len, dest);

    env->ReleaseByteArrayElements(certBytes, raw, JNI_ABORT);

    return true;
}
bool tsl::security::getCertHash(uint8_t out[32]){
    ATTACH
    bool success = verify_signature_universal(env, out);
    DETACH
    if(!success){
        auto apk = ApkSigChecker::getApkPath();
        success = !apk.empty() && ApkSigChecker::verify(apk, out);
    }
    return success;
}
std::string tsl::android::getApkPath(){
    return ApkSigChecker::getApkPath();
}
#elif defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOSERVICE
#define NOMCX
#define NOIME
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")


namespace {
    const wchar_t* getModulePath() {
        static wchar_t path[MAX_PATH];
        HMODULE hModule = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&getModulePath,
            &hModule
        );
        GetModuleFileNameW(hModule, path, MAX_PATH);
        return path;
    }

    wchar_t* charToWChar(const char* str) {
        if (!str) return nullptr;

        size_t len = strlen(str) + 1;
        wchar_t* wstr = new wchar_t[len];

        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, (int)len);
        return wstr;
    }

    // Convert byte to 2 hex characters
    void byteToHex(unsigned char byte, char* hex) {
        const char* hexChars = "0123456789ABCDEF";
        hex[0] = hexChars[(byte >> 4) & 0x0F];
        hex[1] = hexChars[byte & 0x0F];
    }

    // Convert binary thumbprint to hex string and compare
    bool thumbprintMatches(const BYTE* thumbprint, DWORD thumbprintSize, const char* expectedHex) {
        if (thumbprintSize != 20) return false; // SHA-1 is always 20 bytes

        char hexStr[41] = { 0 }; // 20 bytes * 2 chars + null terminator
        for (DWORD i = 0; i < thumbprintSize; i++) {
            byteToHex(thumbprint[i], hexStr + (i * 2));
        }

        // Case-insensitive comparison
        for (int i = 0; i < 40; i++) {
            char c1 = hexStr[i];
            char c2 = expectedHex[i];

            // Convert to uppercase for comparison
            if (c1 >= 'a' && c1 <= 'f') c1 -= 32;
            if (c2 >= 'a' && c2 <= 'f') c2 -= 32;

            if (c1 != c2) return false;
        }

        return true;
    }
}
// Replace the thumbprint check with SHA-256
bool getThumbprintSHA256(PCCERT_CONTEXT pCertContext, uint8_t out[32]) {
    DWORD size = 32;
    return CertGetCertificateContextProperty(
        pCertContext,
        CERT_SHA256_HASH_PROP_ID,  // SHA-256 instead of CERT_HASH_PROP_ID
        out,
        &size
    ) && size == 32;
}

// modified verifyMySignature to just return the hash:
bool tsl::security::getCertHash(uint8_t out[32]) {
    const wchar_t* modulePath = getModulePath();

    HCERTSTORE hStore = NULL;
    HCRYPTMSG  hMsg   = NULL;
    DWORD dwEncoding  = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

    if (!CryptQueryObject(
        CERT_QUERY_OBJECT_FILE, modulePath,
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0, &dwEncoding, NULL, NULL, &hStore, &hMsg, NULL))
        return false;

    DWORD dwSignerInfo = 0;
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &dwSignerInfo);
    auto* pSignerInfo = (CMSG_SIGNER_INFO*)new uint8_t[dwSignerInfo];
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &dwSignerInfo);

    CERT_INFO certInfo = {};
    certInfo.Issuer       = pSignerInfo->Issuer;
    certInfo.SerialNumber = pSignerInfo->SerialNumber;

    PCCERT_CONTEXT pCert = CertFindCertificateInStore(
        hStore, dwEncoding, 0,
        CERT_FIND_SUBJECT_CERT, &certInfo, NULL);

    bool ok = false;
    if (pCert) {
        ok = getThumbprintSHA256(pCert, out);
        CertFreeCertificateContext(pCert);
    }

    delete[](uint8_t*)pSignerInfo;
    if (hStore) CertCloseStore(hStore, 0);
    if (hMsg)   CryptMsgClose(hMsg);
    return ok;
}

// Company name for signature verification (obfuscated at top)
static const char* const EXPECTED_SIGNER = "The Secret Laboratory";
// Certificate thumbprint for additional security (optional but recommended)
static const char* const EXPECTED_THUMBPRINT = "A1B4371191ABD7BBEECD494DA15CD310259406CB";



bool tsl::security::verifyMySignature2() {
    const wchar_t* modulePath = getModulePath();

    // Step 1: Verify signature validity
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = modulePath;

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(NULL, &policyGUID, &trustData);

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policyGUID, &trustData);

    if (status != ERROR_SUCCESS) {
        return false;
    }

    // Step 2: Verify certificate subject and thumbprint
    HCERTSTORE hStore = NULL;
    HCRYPTMSG hMsg = NULL;
    DWORD dwEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

    BOOL result = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE,
        modulePath,
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0,
        &dwEncoding,
        NULL, NULL,
        &hStore,
        &hMsg,
        NULL
    );

    if (!result) {
        return false;
    }

    DWORD dwSignerInfo = 0;
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &dwSignerInfo);

    CMSG_SIGNER_INFO* pSignerInfo = (CMSG_SIGNER_INFO*)new unsigned char[dwSignerInfo];
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &dwSignerInfo);

    CERT_INFO certInfo = { 0 };
    certInfo.Issuer = pSignerInfo->Issuer;
    certInfo.SerialNumber = pSignerInfo->SerialNumber;

    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
        hStore,
        dwEncoding,
        0,
        CERT_FIND_SUBJECT_CERT,
        &certInfo,
        NULL
    );

    bool isValid = false;
    if (pCertContext) {
        // Check 1: Verify subject name
        wchar_t szName[256];
        CertGetNameStringW(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            NULL,
            szName,
            256
        );

        wchar_t* expectedWide = charToWChar(EXPECTED_SIGNER);
        bool nameMatches = false;
        if (expectedWide) {
            nameMatches = (wcsstr(szName, expectedWide) != NULL);
            delete[] expectedWide;
        }

        // Check 2: Verify thumbprint using SHA1 hash property
        BYTE thumbprint[20];
        DWORD thumbprintSize = sizeof(thumbprint);
        bool thumbprintOk = false;

        if (CertGetCertificateContextProperty(
            pCertContext,
            CERT_HASH_PROP_ID,
            thumbprint,
            &thumbprintSize)) {
            thumbprintOk = thumbprintMatches(thumbprint, thumbprintSize, EXPECTED_THUMBPRINT);
        }

        // Both checks must pass
        isValid = nameMatches && thumbprintOk;

        CertFreeCertificateContext(pCertContext);
    }

    delete[](unsigned char*)pSignerInfo;
    if (hStore) CertCloseStore(hStore, 0);
    if (hMsg) CryptMsgClose(hMsg);

    return isValid;
}

// In SignatureVerifier.cpp - macOS section

#elif defined  (__APPLE__)&& !defined(OS_IOS)
#include <mach-o/dyld.h>
#include <Security/Security.h>
#include <Security/SecCode.h>
#include <Security/SecRequirement.h>
#include <cstring>

// List available signing identities
//security find - identity - v - p codesigning

// For production certificate, use the full name from Apple
static const char* const EXPECTED_SIGNER = "Developer ID Application: The Secret Laboratory";

namespace {
    const char* getModulePath() {
        static char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return path;
        }
        return "";
    }

    // Optional: Use requirement string for stricter validation
    bool verifyWithRequirement(SecStaticCodeRef staticCode) {
        SecRequirementRef requirement = NULL;
        CFStringRef reqString = CFSTR("anchor apple generic and certificate leaf[subject.CN] = \"The Secret Laboratory\"");

        OSStatus status = SecRequirementCreateWithString(reqString, kSecCSDefaultFlags, &requirement);
        if (status != errSecSuccess) {
            return false;
        }

        status = SecStaticCodeCheckValidity(staticCode, kSecCSDefaultFlags, requirement);
        CFRelease(requirement);

        return (status == errSecSuccess);
    }

    // Verify code signature with detailed checks
    bool verifyCodeSignatureDetailed(SecStaticCodeRef staticCode) {
        // Check 1: Verify signature validity
        OSStatus status = SecStaticCodeCheckValidity(
            staticCode,
            kSecCSCheckAllArchitectures | kSecCSCheckNestedCode,
            NULL
        );

        if (status != errSecSuccess) {
            return false;
        }

        // Check 2: Get signing information
        CFDictionaryRef signingInfo = NULL;
        status = SecCodeCopySigningInformation(
            staticCode,
            kSecCSSigningInformation,
            &signingInfo
        );

        if (status != errSecSuccess) {
            return false;
        }

        bool isValid = false;

        // Check if we have a certificate chain (not ad-hoc)
        auto certChain = static_cast<CFArrayRef>(CFDictionaryGetValue(
            signingInfo,
            kSecCodeInfoCertificates
        ));

        if (certChain && CFArrayGetCount(certChain) > 0) {
            // We have a real certificate (not ad-hoc signing)

            // Get the signing certificate
            auto cert = SecCertificateRef(CFArrayGetValueAtIndex(certChain, 0));

            // Get certificate subject
            CFStringRef subjectSummary = nullptr;
            subjectSummary = SecCertificateCopySubjectSummary(cert);

            if (subjectSummary) {
                char subjectStr[256];
                CFStringGetCString(subjectSummary, subjectStr, sizeof(subjectStr), kCFStringEncodingUTF8);

                // Check if subject contains our company name
                const char* expectedSigner = EXPECTED_SIGNER;
                if (strstr(subjectStr, expectedSigner) != nullptr) {
                    isValid = true;
                }

                CFRelease(subjectSummary);
            }
        }
        else {
            // Ad-hoc signing - for development/testing
            // In production, you would reject this
#ifdef DEBUG
            isValid = true; // Allow ad-hoc in debug builds
#else
            isValid = false; // Reject ad-hoc in release builds
#endif
        }

        CFRelease(signingInfo);
        return isValid;
    }
}

bool tsl::security::verifyMySignature2() {


    const char* modulePath = getModulePath();

    SecStaticCodeRef staticCode = nullptr;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        nullptr,
        (const UInt8*)modulePath,
        std::strlen(modulePath),
        false
    );

    if (!url) return false;

    OSStatus status = SecStaticCodeCreateWithPath(
        url,
        kSecCSDefaultFlags,
        &staticCode
    );
    CFRelease(url);

    if (status != errSecSuccess) {
        return false;
    }

    // Perform detailed verification
    bool isValid = verifyCodeSignatureDetailed(staticCode);

    CFRelease(staticCode);

    return isValid;
}

bool tsl::security::getCertHash(uint8_t out[32]) {
    SecCodeRef code = nullptr;
    OSStatus status = SecCodeCopySelf(kSecCSDefaultFlags, &code);
    if (status != errSecSuccess) return false;

    CFDictionaryRef signingInfo = nullptr;
    status = SecCodeCopySigningInformation(
        code, kSecCSSigningInformation, &signingInfo);
    CFRelease(code);
    if (status != errSecSuccess) return false;

    bool ok = false;
    auto certChain = (CFArrayRef)CFDictionaryGetValue(
        signingInfo, kSecCodeInfoCertificates);

    if (certChain && CFArrayGetCount(certChain) > 0) {
        auto cert = (SecCertificateRef)
            CFArrayGetValueAtIndex(certChain, 0);

        // get raw DER bytes of the cert
        CFDataRef derData = SecCertificateCopyData(cert);
        if (derData) {
            const uint8_t* der = CFDataGetBytePtr(derData);
            CFIndex derLen     = CFDataGetLength(derData);
            // SHA-256 of raw DER — same as what Android computes
            tsl::sha256::sha256(der, (size_t)derLen, out);
            CFRelease(derData);
            ok = true;
        }
    }

    CFRelease(signingInfo);
    return ok;
}
#else
// Linux or other platforms - no verification
bool tsl::security::verifyMySignature2() {
    return true;
}
bool tsl::security::getCertHash(uint8_t out[32]) {
    std::memset(out, 0, 32);
    return false;
}
#endif
