package me.rocks.pocketanalog;

import android.app.Activity;
import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.ProductDetailsResponseListener;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesResponseListener;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryProductDetailsResult;
import com.android.billingclient.api.QueryPurchasesParams;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import static me.rocks.pocketanalog.MyApplication.java_ofl;

import androidx.annotation.NonNull;

/**
 * Handles all the interactions with Play Store (via Billing library), maintains connection to
 * it through BillingClient and caches temporary states/data if needed
 * Updated for Google Play Billing Library 8.0.0 - Android API 23 Compatible
 */
public class BillingManager implements PurchasesUpdatedListener {

    /**
     * A reference to BillingClient
     **/
    private BillingClient mBillingClient;

    /**
     * True if billing service is connected now.
     */
    private boolean mIsServiceConnected;

    private final BillingUpdatesListener mBillingUpdatesListener;

    private final Activity mActivity;

    private final List<Purchase> mPurchases = new ArrayList<>();
    private final List<Purchase> mSubscriptions = new ArrayList<>();

    private int mBillingClientResponseCode = BillingClient.BillingResponseCode.SERVICE_UNAVAILABLE;

    // Executor for handling billing operations
    private final ScheduledExecutorService mExecutorService = Executors.newSingleThreadScheduledExecutor();

    // Connection retry policy
    private static final int MAX_RETRY_ATTEMPTS = 3;
    private static final long RETRY_DELAY_MS = 2000;
    private int mConnectionRetryCount = 0;

    private static final String key = "pocketanalog";

    /**
     * Listener to the updates that happen when purchases list was updated or consumption of the
     * item was finished
     */
    public interface BillingUpdatesListener {
        void onBillingClientSetupFinished();
        /**
         * @param fromQuery true only for the result of an explicit
         *     queryPurchases() that Play answered OK -- the one case where
         *     an ABSENT product means "not owned" rather than "not known".
         *     The purchase-flow notification below fires with whatever
         *     mPurchases happens to hold, including on USER_CANCELED, so a
         *     listener must not read absence as a negative there.
         */
        void onPurchasesUpdated(List<Purchase> purchases, boolean fromQuery);
        void onSubscriptionsUpdated(List<Purchase> subscriptions);
        void onPurchaseConsumed(String purchaseToken);
        void onBillingError(BillingResult billingResult);
    }

    public BillingManager(Activity activity, final BillingUpdatesListener updatesListener) {
        mActivity = activity;
        mBillingUpdatesListener = updatesListener;
        initializeBillingClient();
    }

    private void initializeBillingClient() {
        PendingPurchasesParams pp = PendingPurchasesParams.newBuilder().enableOneTimeProducts().build();
        mBillingClient = BillingClient.newBuilder(mActivity)
                .setListener(this)
                .enablePendingPurchases(pp)
                .build();
        startServiceConnection();
    }

    /**
     * Acknowledge a purchase
     */
    public void acknowledgePurchase(Purchase purchase, @Nullable Runnable onComplete) {
        if (mBillingClient == null || !mBillingClient.isReady()) {
            if (onComplete != null) onComplete.run();
            return;
        }

        AcknowledgePurchaseParams acknowledgePurchaseParams =
                AcknowledgePurchaseParams.newBuilder()
                        .setPurchaseToken(purchase.getPurchaseToken())
                        .build();

        mBillingClient.acknowledgePurchase(acknowledgePurchaseParams, billingResult -> {
            if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK) {
                mBillingUpdatesListener.onBillingError(billingResult);
            }
            if (onComplete != null) onComplete.run();
        });
    }

    /**
     * Consume a purchase
     */
    public void consumePurchase(Purchase purchase, @Nullable Runnable onComplete) {
        if (mBillingClient == null || !mBillingClient.isReady()) {
            if (onComplete != null) onComplete.run();
            return;
        }

        ConsumeParams consumeParams = ConsumeParams.newBuilder()
                .setPurchaseToken(purchase.getPurchaseToken())
                .build();

        mBillingClient.consumeAsync(consumeParams, (billingResult, purchaseToken) -> {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                // Remove from local cache - API 23 compatible way
                removePurchaseByToken(mPurchases, purchaseToken);
                mBillingUpdatesListener.onPurchaseConsumed(purchaseToken);
            } else {
                mBillingUpdatesListener.onBillingError(billingResult);
            }
            if (onComplete != null) onComplete.run();
        });
    }

    /**
     * API 23 compatible method to remove purchase by token
     */
    private void removePurchaseByToken(List<Purchase> purchases, String purchaseToken) {
        Iterator<Purchase> iterator = purchases.iterator();
        while (iterator.hasNext()) {
            Purchase purchase = iterator.next();
            if (purchase.getPurchaseToken().equals(purchaseToken)) {
                iterator.remove();
                break;
            }
        }
    }

    /**
     * API 23 compatible method to remove purchase by product list
     */
    private void removePurchaseByProducts(List<Purchase> purchases, List<String> products) {
        Iterator<Purchase> iterator = purchases.iterator();
        while (iterator.hasNext()) {
            Purchase purchase = iterator.next();
            if (purchase.getProducts().equals(products)) {
                iterator.remove();
                break;
            }
        }
    }

    /**
     * Handle a callback that purchases were updated from the Billing library
     */
    @Override
    public void onPurchasesUpdated(BillingResult result, @Nullable List<Purchase> purchases) {
        if (result.getResponseCode() == BillingClient.BillingResponseCode.OK) {
            if (purchases != null) {
                for (Purchase purchase : purchases) {
                    handlePurchase(purchase);
                }
            }
        } else {
            mBillingUpdatesListener.onBillingError(result);
        }

        // Notify listeners
        mBillingUpdatesListener.onPurchasesUpdated(new ArrayList<>(mPurchases), false);
        mBillingUpdatesListener.onSubscriptionsUpdated(new ArrayList<>(mSubscriptions));
    }

    /**
     * Handles the purchase with improved security validation
     */
    private void handlePurchase(Purchase purchase) {
        if (!verifyValidSignature(purchase.getOriginalJson(), purchase.getSignature())) {
            return;
        }

        // Separate in-app purchases from subscriptions
        boolean isSubscription = false;
        for (String productId : purchase.getProducts()) {
            if (isSubscriptionProduct(productId)) {
                isSubscription = true;
                break;
            }
        }

        if (isSubscription) {
            // Remove existing subscription with same product ID and add new one
            removePurchaseByProducts(mSubscriptions, purchase.getProducts());
            mSubscriptions.add(purchase);
        } else {
            // Handle in-app purchases
            removePurchaseByToken(mPurchases, purchase.getPurchaseToken());
            mPurchases.add(purchase);
        }

        if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
            // Grant access and acknowledge
            if (!purchase.isAcknowledged()) {
                acknowledgePurchase(purchase, null);
            }
        } else if (purchase.getPurchaseState() == Purchase.PurchaseState.PENDING) {
            // Show a message: "Payment pending. Features will unlock once confirmed."
        }
    }

    /**
     * Helper method to determine if a product ID represents a subscription
     * Override this method to match your product structure
     */
    protected boolean isSubscriptionProduct(String productId) {
        // Add your subscription product IDs here
        return productId.contains("subscription") ||
                productId.contains("monthly") ||
                productId.contains("yearly") ||
                productId.contains("premium");
    }

    /**
     * Query purchases for both in-app purchases and subscriptions
     */
    public void queryPurchases() {
        queryInAppPurchases();
        querySubscriptions();
    }

    private void queryInAppPurchases() {
        Runnable queryToExecute = () -> {
            QueryPurchasesParams params = QueryPurchasesParams.newBuilder()
                    .setProductType(BillingClient.ProductType.INAPP)
                    .build();
            mBillingClient.queryPurchasesAsync(params, new PurchasesResponseListener() {
                @Override
                public void onQueryPurchasesResponse(@NotNull BillingResult billingResult, @NotNull List<Purchase> purchases) {
                    if (mBillingClient == null) return;

                    if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        mPurchases.clear();
                        for (Purchase purchase : purchases) {
                            handlePurchase(purchase);
                        }
                        mBillingUpdatesListener.onPurchasesUpdated(new ArrayList<>(mPurchases), true);
                    } else {
                        mBillingUpdatesListener.onBillingError(billingResult);
                    }
                }
            });
        };

        executeServiceRequest(queryToExecute);
    }

    private void querySubscriptions() {
        Runnable queryToExecute = new Runnable() {
            @Override
            public void run() {
                QueryPurchasesParams params = QueryPurchasesParams.newBuilder()
                        .setProductType(BillingClient.ProductType.SUBS)
                        .build();
                PurchasesResponseListener listener = new PurchasesResponseListener() {
                    @Override
                    public void onQueryPurchasesResponse(@NonNull BillingResult billingResult, @NonNull List<Purchase> list) {

                    }
                };

                mBillingClient.queryPurchasesAsync(params, new PurchasesResponseListener() {
                    @Override
                    public void onQueryPurchasesResponse(@NotNull BillingResult billingResult, @NotNull List<Purchase> purchases) {
                        if (mBillingClient == null) return;

                        if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                            mSubscriptions.clear();
                            for (Purchase purchase : purchases) {
                                handlePurchase(purchase);
                            }
                            mBillingUpdatesListener.onSubscriptionsUpdated(new ArrayList<>(mSubscriptions));
                        } else {
                            mBillingUpdatesListener.onBillingError(billingResult);
                        }
                    }
                });
            }
        };

        executeServiceRequest(queryToExecute);
    }

    /**
     * Start a purchase flow with enhanced error handling
     */
    public void initiatePurchaseFlow(final ProductDetails productDetails, @Nullable String offerToken) {
        Runnable purchaseFlowRequest = new Runnable() {
            @Override
            public void run() {
                BillingFlowParams.ProductDetailsParams.Builder productDetailsParamsBuilder =
                        BillingFlowParams.ProductDetailsParams.newBuilder()
                                .setProductDetails(productDetails);

                // Add offer token for subscriptions if provided
                if (offerToken != null && !offerToken.isEmpty()) {
                    productDetailsParamsBuilder.setOfferToken(offerToken);
                }

                // API 23 compatible way to create list
                List<BillingFlowParams.ProductDetailsParams> productDetailsParamsList =
                        List.of(productDetailsParamsBuilder.build());

                BillingFlowParams billingFlowParams = BillingFlowParams.newBuilder()
                        .setProductDetailsParamsList(productDetailsParamsList)
                        .build();

                BillingResult billingResult = mBillingClient.launchBillingFlow(mActivity, billingFlowParams);

                if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK) {
                    mBillingUpdatesListener.onBillingError(billingResult);
                    if (!mActivity.isFinishing()) {
                        MyApplication.showToast2(mActivity,
                                "Purchase failed: " + billingResult.getDebugMessage());
                    }
                }
            }
        };

        executeServiceRequest(purchaseFlowRequest);
    }

    /**
     * Overloaded method for backward compatibility
     */
    public void initiatePurchaseFlow(final ProductDetails productDetails) {
        initiatePurchaseFlow(productDetails, null);
    }

    public Activity getActivity() {
        return mActivity;
    }

    /**
     * Clear the resources
     */
    public void destroy() {
        if (mBillingClient != null && mBillingClient.isReady()) {
            mBillingClient.endConnection();
            mBillingClient = null;
        }

        if (mExecutorService != null && !mExecutorService.isShutdown()) {
            mExecutorService.shutdown();
        }

        mPurchases.clear();
        mSubscriptions.clear();
    }

    /**
     * Query product details with improved error handling
     */
    public void queryProductDetailsAsync(final QueryProductDetailsParams params,
                                         final ProductDetailsResponseListener listener) {
        Runnable queryRequest = new Runnable() {
            @Override
            public void run() {

                mBillingClient.queryProductDetailsAsync(params, new ProductDetailsResponseListener() {
                    @Override
                    public void onProductDetailsResponse(@NonNull BillingResult billingResult, @NonNull QueryProductDetailsResult queryProductDetailsResult) {
                        if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK) {
                            mBillingUpdatesListener.onBillingError(billingResult);
                        }
                        listener.onProductDetailsResponse(billingResult, queryProductDetailsResult);
                    }
                });
            }
        };

        executeServiceRequest(queryRequest);
    }

    /**
     * Returns the value Billing client response code or BILLING_MANAGER_NOT_INITIALIZED if the
     * client connection response was not received yet.
     */
    public int getBillingClientResponseCode() {
        return mBillingClientResponseCode;
    }

    /**
     * Check if billing client is ready
     */
    public boolean isReady() {
        return mBillingClient != null && mBillingClient.isReady() && mIsServiceConnected;
    }

    /**
     * Get current purchases
     */
    public List<Purchase> getPurchases() {
        return new ArrayList<>(mPurchases);
    }

    /**
     * Get current subscriptions
     */
    public List<Purchase> getSubscriptions() {
        return new ArrayList<>(mSubscriptions);
    }

    /**
     * Check if user has purchased a specific product - API 23 compatible
     */
    public boolean hasPurchase(String productId) {
        for (Purchase purchase : mPurchases) {
            if (purchase.getProducts().contains(productId) &&
                    purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                return true;
            }
        }
        return false;
    }

    /**
     * Check if user has active subscription - API 23 compatible
     */
    public boolean hasSubscription(String productId) {
        for (Purchase purchase : mSubscriptions) {
            if (purchase.getProducts().contains(productId) &&
                    purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                return true;
            }
        }
        return false;
    }

    private void startServiceConnection() {
        startServiceConnection(null);
    }

    private void startServiceConnection(@Nullable final Runnable executeOnSuccess) {
        if (mBillingClient == null) {
            initializeBillingClient();
            return;
        }

        mBillingClient.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(@NotNull BillingResult billingResult) {
                mBillingClientResponseCode = billingResult.getResponseCode();

                if (mBillingClientResponseCode == BillingClient.BillingResponseCode.OK) {
                    mIsServiceConnected = true;
                    mConnectionRetryCount = 0; // Reset retry count on successful connection

                    // Query existing purchases after successful connection
                    queryPurchases();

                    mBillingUpdatesListener.onBillingClientSetupFinished();

                    if (executeOnSuccess != null) {
                        executeOnSuccess.run();
                    }
                } else {
                    mBillingUpdatesListener.onBillingError(billingResult);
                }
            }

            @Override
            public void onBillingServiceDisconnected() {
                mIsServiceConnected = false;

                // Implement retry policy
                if (mConnectionRetryCount < MAX_RETRY_ATTEMPTS) {
                    mConnectionRetryCount++;

                    mExecutorService.schedule(new Runnable() {
                        @Override
                        public void run() {
                            if (mActivity != null && !mActivity.isFinishing()) {
                                startServiceConnection(executeOnSuccess);
                            }
                        }
                    }, RETRY_DELAY_MS * mConnectionRetryCount, TimeUnit.MILLISECONDS);
                } else {
                    // Max retries reached, notify error
                    BillingResult errorResult = BillingResult.newBuilder()
                            .setResponseCode(BillingClient.BillingResponseCode.SERVICE_UNAVAILABLE)
                            .setDebugMessage("Failed to connect after " + MAX_RETRY_ATTEMPTS + " attempts")
                            .build();
                    mBillingUpdatesListener.onBillingError(errorResult);
                }
            }
        });
    }

    private void executeServiceRequest(final Runnable runnable) {
        if (mIsServiceConnected && mBillingClient != null && mBillingClient.isReady()) {
            runnable.run();
        } else {
            // If billing service was disconnected, we try to reconnect
            startServiceConnection(runnable);
        }
    }

    /**
     * Verifies that the purchase was signed correctly for this developer's public key.
     * <p>Note: It's strongly recommended to perform such check on your backend since hackers can
     * replace this method with "constant true" if they decompile/rebuild your app.
     * </p>
     */
    private boolean verifyValidSignature(String signedData, String signature) {
        try {
            return me.rocks.pocketanalog.Security.verifyPurchase(
                    java_ofl(),
                    signedData,
                    signature
            );
        } catch (IOException e) {
            return false;
        }
    }
}