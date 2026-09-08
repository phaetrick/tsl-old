// LicenseChecker.h
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace tsl {

    // Forward declarations
    class OSCredentialStore;
    class DeviceFingerprint;
    class HttpClient;

    // Device information structure
    struct DeviceInfo {
        std::string deviceId;
        std::string deviceName;
        std::string registeredAt;
        std::string lastSeenAt;
        int daysInactive = 0;
    };

    // Validation result structure
    struct ValidationResult {
        bool success = false;
        std::string status;
        std::string action;
        int statusCode = -1;
       
        // Trial information
        bool hasTrial = false;
        int daysRemaining = 0;
        int totalTrialDays = 0;
        int expiredDaysAgo = 0;
        std::string expiresAt;

        // Device management
        bool deviceLimitReached = false;
        int registeredDevices = 0;
        int maxDevices = 0;
        int availableDevices = 0;
        std::vector<DeviceInfo> devices;
    };

    // Result of inspecting the locally stored offline licence token.
    //
    // The token lets an already-activated machine keep working with no network.
    // It is signed by the server with Ed25519 and the plugin embeds only the
    // PUBLIC half, so it cannot be forged by anyone disassembling the binary —
    // which an HMAC scheme sharing one secret would not survive.
    struct TokenStatus {
        bool present = false;    // a token exists in the credential store
        bool valid = false;      // signature, device, product all good and unexpired
        bool expired = false;    // past its expiry
        bool inGrace = false;    // expired but still inside the offline grace period
        bool refreshDue = false; // worth contacting the server when convenient
        bool tampered = false;   // bad signature, wrong device/product, or clock rolled back

        int daysRemaining = 0;      // until expiry (negative once expired)
        int graceDaysRemaining = 0; // until the grace period runs out
        int purchaseType = -1;      // 0 = permanent, 1 = trial
        std::string reason;         // human-readable, for logs and dialogs

        // True when the app may run without contacting the server at all.
        bool usable() const { return valid || inGrace; }
    };

    class LicenseChecker {
    public:
        // Connectivity status
        struct ConnectivityStatus {
            bool internetAvailable = false;
            bool serverReachable = false;
            std::string statusMessage;
        };

        // Constructor & Destructor
		LicenseChecker(const std::string& baseUrl, int port = 0, bool useHttps = true, int productNumber = 1,
            const std::string& appName = "LicenseChecker", std::function<void(const std::string&)> logger = nullptr);
        ~LicenseChecker();

        // Delete copy constructor and assignment operator
        LicenseChecker(const LicenseChecker&) = delete;
        LicenseChecker& operator=(const LicenseChecker&) = delete;

        // Authentication methods
        int login(const std::string& username, const std::string& password);
        
        bool hasPrivateKey();
        bool clearAll() const;

        // --- Offline licence token ---------------------------------------
        // Inspect the stored token without any network access. Cheap; safe to
        // call on every startup before deciding whether to go online at all.
        TokenStatus checkStoredToken() const;
        TokenStatus checkStoredToken(int productNumber_) const;
        // Discard the stored token (sign-out, or a server verdict of "stop").
        void clearLicenseToken() const;
        // Product validation methods
        bool validateProduct(const int productNumber);
        ValidationResult validateProductDetailed(const int productNumber,
            const std::function<void(const std::string& result)>& logger = nullptr);

        // Connectivity check methods
        bool isInternetConnected() const;
        bool isServerReachable() const;
        ConnectivityStatus checkConnectivity() const;
        bool canConnectToLicenseServer() const;
        std::string lastStatusMessage;
        std::string lastErrorDetails;
        int lastHttpStatusCode{};
        int waitTimeMs{};
    private:
        std::function<void(const std::string&)> logger;
        // Private members
        std::unique_ptr<HttpClient> httpClient;
        std::unique_ptr<OSCredentialStore> credStore;
        std::unique_ptr<DeviceFingerprint> deviceFingerPrint;
        std::string baseUrl;
        std::string host;
        bool useHttps;
        int port;
		int productNumber;
       
        // Private helper methods
        void parseBaseUrl(const std::string& url);       
     };
} // namespace tsl