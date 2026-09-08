#if defined(STANDALONE_MODE)
#import <StoreKit/StoreKit.h>
#include "app.h"

static NSString* const kProductIDPro = @"com.thesecretlaboratory.grainstorm.pro";
static NSString* const kProductIDAU  = @"com.thesecretlaboratory.grainstorm.au";

@interface TSLStoreKitManager : NSObject <SKProductsRequestDelegate, SKPaymentTransactionObserver>
@property (nonatomic, assign) tsl::AppState* appState;
@end

@implementation TSLStoreKitManager

- (void)productsRequest:(SKProductsRequest*)request didReceiveResponse:(SKProductsResponse*)response {
    NSLog(@"[TSL] productsRequest response: %lu products, invalid: %@",
          (unsigned long)response.products.count, response.invalidProductIdentifiers);
    if (response.products.count > 0) {
        for (SKProduct* product in response.products) {
            SKPayment* payment = [SKPayment paymentWithProduct:product];
            [[SKPaymentQueue defaultQueue] addPayment:payment];
        }
    } else {
        if (self.appState->onPurchaseComplete)
            self.appState->onPurchaseComplete(false, "");
    }
}

- (void)request:(SKRequest*)request didFailWithError:(NSError*)error {
    NSLog(@"[TSL] request failed: %@", error.localizedDescription);
    if (self.appState->onPurchaseComplete)
        self.appState->onPurchaseComplete(false, "");
}

- (void)paymentQueue:(SKPaymentQueue*)queue updatedTransactions:(NSArray<SKPaymentTransaction*>*)transactions {
    for (SKPaymentTransaction* transaction in transactions) {
        std::string productId = transaction.payment.productIdentifier.UTF8String;
        switch (transaction.transactionState) {
            case SKPaymentTransactionStatePurchased:
            case SKPaymentTransactionStateRestored:
                [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
                if (self.appState->onPurchaseComplete)
                    self.appState->onPurchaseComplete(true, productId);
                break;
            case SKPaymentTransactionStateFailed:
                [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
                if (self.appState->onPurchaseComplete)
                    self.appState->onPurchaseComplete(false, productId);
                break;
            default:
                break;
        }
    }
}

@end

static TSLStoreKitManager* getOrCreateManager(tsl::AppState* appState) {
    if (!appState->iosStoreKitManager) {
        TSLStoreKitManager* mgr = [[TSLStoreKitManager alloc] init];
        mgr.appState = appState;
        appState->iosStoreKitManager = (__bridge_retained void*)mgr;
        [[SKPaymentQueue defaultQueue] addTransactionObserver:mgr];
    }
    return (__bridge TSLStoreKitManager*)appState->iosStoreKitManager;
}

static void startPurchase(tsl::AppState* appState, NSString* productId) {
    dispatch_async(dispatch_get_main_queue(), ^{
        TSLStoreKitManager* mgr = getOrCreateManager(appState);
        if (![SKPaymentQueue canMakePayments]) {
            if (appState->onPurchaseComplete)
                appState->onPurchaseComplete(false, productId.UTF8String);
            return;
        }
        NSLog(@"[TSL] starting products request for %@", productId);
        SKProductsRequest* request = [[SKProductsRequest alloc]
            initWithProductIdentifiers:[NSSet setWithObject:productId]];
        request.delegate = mgr;
        [request start];
    });
}

void tsl::AppState::purchaseAU() {
    startPurchase(this, kProductIDAU);
}

void tsl::AppState::purchasePro() {
    startPurchase(this, kProductIDPro);
}

void tsl::AppState::restorePurchases() {
    dispatch_async(dispatch_get_main_queue(), ^{
        getOrCreateManager(this);
        [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
    });
}

#endif

void tsl::AppState::checkAndSetupPurchase() {
#if defined(STANDALONE_MODE)
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:@"pro_purchased"])
        this->dofastrender = true;
    if ([defaults boolForKey:@"au_purchased"])
        this->auv3Purchased = true;
#endif
    this->readyForUiSetup.release();
}

void tsl::app::onIAPComplete(tsl::AppState* appState, const std::string& productId) {
#if defined(STANDALONE_MODE)
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    if (productId == "com.thesecretlaboratory.grainstorm.pro") {
        [defaults setBool:YES forKey:@"pro_purchased"];
        [defaults synchronize];
        appState->dofastrender = true;
    } else if (productId == "com.thesecretlaboratory.grainstorm.au") {
        [defaults setBool:YES forKey:@"au_purchased"];
        [defaults synchronize];
        appState->auv3Purchased = true;
    }
#endif
}