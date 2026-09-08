package me.rocks.pocketanalog;



/**
 * An interface that provides an access to BillingLibrary methods
 */
public interface BillingProvider {
    BillingManager getBillingManager();
    boolean isPremiumPurchased();
}
