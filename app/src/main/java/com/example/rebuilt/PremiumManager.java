package com.example.rebuilt;

public final class PremiumManager {

    private static final PremiumManager INSTANCE = new PremiumManager();
    private static final String LOCAL_ENTITLEMENT = "chroma_toons_local_premium";

    private PremiumManager() {
    }

    public static PremiumManager get() {
        return INSTANCE;
    }

    public boolean isPremium() {
        return true;
    }

    public boolean isUnlocked(Feature feature) {
        return true;
    }

    public String activeEntitlement() {
        return LOCAL_ENTITLEMENT;
    }

    public void purchasePro(Runnable onComplete) {
        complete(onComplete);
    }

    public void restorePurchases(Runnable onComplete) {
        complete(onComplete);
    }

    private static void complete(Runnable callback) {
        if (callback != null) {
            callback.run();
        }
    }

    public enum Feature {
        RIG_EDITOR,
        CUSTOM_ANIMATIONS,
        MEDIA_EDITOR,
        CLOUD_CHARACTERS,
        AD_FREE
    }
}
