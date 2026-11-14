package com.example.rebuilt;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public final class FeatureActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_feature);

        String title = getIntent().getStringExtra(MainActivity.EXTRA_TITLE);
        String body = getIntent().getStringExtra(MainActivity.EXTRA_BODY);

        TextView titleView = findViewById(R.id.feature_title);
        TextView bodyView = findViewById(R.id.feature_body);
        TextView statusView = findViewById(R.id.feature_status);

        titleView.setText(title != null ? title : getString(R.string.app_name));
        bodyView.setText(body != null ? body : getString(R.string.feature_default_body));
        statusView.setText(PremiumManager.get().isPremium()
                ? R.string.feature_unlocked
                : R.string.feature_locked);

        findViewById(R.id.feature_back).setOnClickListener(v -> finish());
    }
}
