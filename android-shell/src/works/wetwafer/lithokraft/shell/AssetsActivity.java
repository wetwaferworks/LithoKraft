package works.wetwafer.lithokraft.shell;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.webkit.ConsoleMessage;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.webkit.Profile;
import androidx.webkit.WebViewAssetLoader;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * LithoKraft Shell — Leg B entry point (shell v1.6): the same bundled
 * web package, served from the androidx.webkit WebViewAssetLoader
 * origin https://appassets.androidplatform.net/assets/ instead of the
 * in-app loopback HTTPS server.
 *
 * Why this leg exists: the appassets origin needs no socket, no TLS
 * and no certificate at all — the WebView platform itself vouches for
 * the origin. It is the cert-free control in the isolation
 * experiment: if WebView 155 grants cross-origin isolation to ANY
 * locally-served origin class, this special origin is a candidate
 * (it is the origin Android provides for exactly this purpose, and
 * features like service workers treat it as a secure context).
 *
 * Whether the isolation *processing* runs for these responses is the
 * open question — and the honest prior is caution: the v1.0–v1.3
 * shells served intercepted https://app.local/ responses with the
 * same headers and got NO isolation processing in WebView. This leg
 * differs in the origin (a platform-sanctioned https domain rather
 * than a made-up one) and in the serving machinery (the platform's
 * own asset loader, with the isolation headers set on every response
 * by our PathHandler). If the grant is refused here too, lkboot's
 * blocked state reports it on the page exactly as in the server leg —
 * a clean datum either way. That is all this activity claims.
 *
 * Second launcher icon, labeled "LithoKraft (appassets)"; the main
 * "LithoKraft Shell" icon keeps the HTTPS-server legs (v1.5 loopback
 * profile, or the v1.6 public-cert profile when its keystore asset is
 * bundled — see MainActivity).
 *
 * v1.7: this leg gets the same cross-origin-isolated allowlist
 * wiring as MainActivity (Profile#setCrossOriginIsolatedAllowlist,
 * AndroidX WebKit 1.18), naming this leg's exact origin, and the
 * same Document-Isolation-Policy header on every response — see
 * MainActivity's header for the full rationale. Wiring is
 * feature-gated and failure-soft; onPageFinished reports the
 * outcome into the page log on every boot.
 */
public class AssetsActivity extends Activity {

    private static final String TAG = "LithoKraftShell";
    private static final String DOMAIN = "appassets.androidplatform.net";

    private WebView webView;
    private boolean diagMode;
    private WebViewAssetLoader assetLoader;
    // v1.7 allowlist wiring outcome, read by the onPageFinished
    // instrumentation so every boot reports it into the page log.
    private boolean coiFeatureSupported;
    private String coiAllowlistReadback = "(not attempted)";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        webView = new WebView(this);
        webView.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        setContentView(webView);
        enterImmersive();

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        settings.setSupportZoom(false);
        settings.setBuiltInZoomControls(false);

        assetLoader = new WebViewAssetLoader.Builder()
                .setDomain(DOMAIN)
                .addPathHandler("/assets/", new BundlePathHandler())
                .build();

        webView.setWebViewClient(new AssetsClient());
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage msg) {
                Log.i(TAG, msg.message() + " (" + msg.sourceId() + ":" + msg.lineNumber() + ")");
                return true;
            }
        });

        diagMode = assetExists("lk-diag.flag");
        // v1.7: allowlist wiring before the first load; failure-soft.
        applyCrossOriginIsolatedAllowlist();
        String entry = "https://" + DOMAIN + "/assets/gl.html" + (diagMode ? "?diag=1" : "");
        Log.i(TAG, "entry: " + entry + " (diag=" + diagMode + ", profile=appassets)");
        webView.loadUrl(entry);
    }

    /**
     * v1.7 — name this leg's exact origin on the WebView profile's
     * cross-origin-isolated allowlist (AndroidX WebKit 1.18,
     * Profile#setCrossOriginIsolatedAllowlist), paired with the
     * Document-Isolation-Policy header the path handler sends on
     * every response. The bare origin and its explicit :443 form
     * are both offered; if the API's parser rejects the combined
     * set, the bare origin is retried alone. Any unsupported
     * feature or failure leaves the shell behaving exactly as
     * v1.6; nothing here may crash the activity (this door already
     * carries one unexplained device crash — add no new risk).
     */
    private void applyCrossOriginIsolatedAllowlist() {
        try {
            if (!WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE)) {
                coiAllowlistReadback = "(skipped: MULTI_PROFILE unsupported)";
                Log.i(TAG, "coi allowlist: " + coiAllowlistReadback);
                return;
            }
            Profile profile = WebViewCompat.getProfile(webView);
            if (profile == null) {
                coiAllowlistReadback = "(skipped: profile unavailable)";
                Log.i(TAG, "coi allowlist: " + coiAllowlistReadback);
                return;
            }
            coiFeatureSupported = WebViewFeature.isFeatureSupported(
                    WebViewFeature.CROSS_ORIGIN_ISOLATED_ALLOWLIST);
            Log.i(TAG, "coi allowlist: CROSS_ORIGIN_ISOLATED_ALLOWLIST supported=" + coiFeatureSupported);
            if (!coiFeatureSupported) {
                coiAllowlistReadback = "(skipped: allowlist feature unsupported)";
                return;
            }
            Set<String> origins = new HashSet<>();
            origins.add("https://" + DOMAIN);
            origins.add("https://" + DOMAIN + ":443");
            try {
                profile.setCrossOriginIsolatedAllowlist(origins);
            } catch (Throwable t) {
                Log.w(TAG, "coi allowlist: combined set rejected (" + t.getMessage()
                        + "); retrying bare origin only");
                origins.remove("https://" + DOMAIN + ":443");
                profile.setCrossOriginIsolatedAllowlist(origins);
            }
            try {
                coiAllowlistReadback = String.valueOf(profile.getCrossOriginIsolatedAllowlist());
            } catch (Throwable t) {
                coiAllowlistReadback = "(readback failed: " + t.getMessage() + ")";
            }
            Log.i(TAG, "coi allowlist: readback " + coiAllowlistReadback);
        } catch (Throwable t) {
            coiAllowlistReadback = "(wiring failed: " + t.getMessage() + ")";
            Log.w(TAG, "coi allowlist: wiring failed, continuing with v1.6 behavior: " + t);
        }
    }

    private boolean assetExists(String name) {
        try {
            String[] names = getAssets().list("");
            if (names != null) {
                for (String n : names) {
                    if (n.equals(name)) return true;
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "asset list failed: " + e.getMessage());
        }
        return false;
    }

    private void enterImmersive() {
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersive();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (webView != null) {
            webView.onPause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (webView != null) {
            webView.onResume();
        }
    }

    @Override
    protected void onDestroy() {
        if (webView != null) {
            webView.destroy();
            webView = null;
        }
        super.onDestroy();
    }

    private class AssetsClient extends WebViewClient {

        @Override
        public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
            return assetLoader.shouldInterceptRequest(request.getUrl());
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            // Keep navigation inside the shell; block anything off-origin.
            String host = request.getUrl().getHost();
            return !DOMAIN.equals(host);
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            super.onPageFinished(view, url);
            // v1.7 field instrumentation — runs on EVERY boot, diag
            // mode or not: the allowlist wiring outcome and the
            // page's own isolation verdict, in the page log.
            String coiHead = "[shell] coi-allowlist feature=" + coiFeatureSupported
                    + " allowlist=" + coiAllowlistReadback;
            Log.i(TAG, coiHead);
            String coiJs = "(function(){var el=document.getElementById('log');if(!el)return;"
                    + "var p=" + jsString(coiHead)
                    + " + ' page crossOriginIsolated=' + window.crossOriginIsolated"
                    + " + ' SharedArrayBuffer=' + (typeof SharedArrayBuffer)"
                    + " + ' origin=' + location.origin;"
                    + "el.textContent += p + '\\n';})()";
            view.evaluateJavascript(coiJs, null);
            if (!diagMode) return;
            // Same diag probe as the server leg (shell v1.4+): WebView
            // build + the page's isolation state, into the page log.
            String pkg = "(unknown)";
            try {
                android.content.pm.PackageInfo pi = WebView.getCurrentWebViewPackage();
                if (pi != null) pkg = pi.packageName + " " + pi.versionName;
            } catch (Exception e) {
                pkg = "(unavailable: " + e.getMessage() + ")";
            }
            String ua = view.getSettings().getUserAgentString();
            String line = "[shell] diag shell (appassets); webview package: " + pkg + "; shell ua: " + ua;
            Log.i(TAG, line);
            String js = "(function(){var el=document.getElementById('log');if(!el)return;"
                    + "el.textContent += " + jsString(line) + " + '\\n';"
                    + "var p='[shell] crossOriginIsolated=' + window.crossOriginIsolated"
                    + " + ' SharedArrayBuffer=' + (typeof SharedArrayBuffer)"
                    + " + ' Worker=' + (typeof Worker)"
                    + " + ' origin=' + location.origin;"
                    + "el.textContent += p + '\\n';})()";
            view.evaluateJavascript(js, null);
        }

        private String jsString(String s) {
            return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'";
        }
    }

    /**
     * Serves the bundled web package from APK assets with the
     * cross-origin-isolation header set on EVERY response — the same
     * set, the same values as MainActivity's LocalHttpsServer (the
     * stock AssetsPathHandler cannot set response headers, which is
     * why this handler exists). Path arrives as the request path under
     * the registered "/assets/" prefix; normalize defensively (the
     * loader may hand us the path with or without the prefix).
     */
    private class BundlePathHandler implements WebViewAssetLoader.PathHandler {

        @Override
        public WebResourceResponse handle(String path) {
            String assetName = path == null ? "" : path;
            if (assetName.startsWith("/")) assetName = assetName.substring(1);
            if (assetName.startsWith("assets/")) assetName = assetName.substring("assets/".length());
            if (assetName.isEmpty()) assetName = "gl.html";
            int q = assetName.indexOf('?');
            if (q >= 0) assetName = assetName.substring(0, q);
            if (assetName.contains("..")) {
                return notFound();
            }
            byte[] body;
            try (InputStream in = getAssets().open(assetName)) {
                body = readAll(in);
            } catch (IOException e) {
                Log.w(TAG, "appassets miss: " + assetName);
                return notFound();
            }
            String mime = mimeTypeFor(assetName);
            boolean text = mime.startsWith("text/") || mime.equals("application/json");
            WebResourceResponse resp = new WebResourceResponse(
                    mime, text ? "utf-8" : null, new ByteArrayInputStream(body));
            Map<String, String> headers = new HashMap<>();
            headers.put("Cross-Origin-Opener-Policy", "same-origin");
            headers.put("Cross-Origin-Embedder-Policy", "require-corp");
            headers.put("Cross-Origin-Resource-Policy", "same-origin");
            headers.put("Document-Isolation-Policy", "isolate-and-require-corp");
            headers.put("Cache-Control", "no-store");
            resp.setResponseHeaders(headers);
            return resp;
        }

        private WebResourceResponse notFound() {
            WebResourceResponse resp = new WebResourceResponse(
                    "text/plain", "utf-8", new ByteArrayInputStream(new byte[0]));
            resp.setStatusCodeAndReasonPhrase(404, "Not Found");
            Map<String, String> headers = new HashMap<>();
            headers.put("Cross-Origin-Opener-Policy", "same-origin");
            headers.put("Cross-Origin-Embedder-Policy", "require-corp");
            headers.put("Cross-Origin-Resource-Policy", "same-origin");
            headers.put("Document-Isolation-Policy", "isolate-and-require-corp");
            headers.put("Cache-Control", "no-store");
            resp.setResponseHeaders(headers);
            return resp;
        }

        private byte[] readAll(InputStream in) throws IOException {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            byte[] chunk = new byte[65536];
            int n;
            while ((n = in.read(chunk)) >= 0) buf.write(chunk, 0, n);
            return buf.toByteArray();
        }

        private String mimeTypeFor(String name) {
            String n = name.toLowerCase();
            if (n.endsWith(".html") || n.endsWith(".htm")) return "text/html";
            if (n.endsWith(".js")) return "text/javascript";
            if (n.endsWith(".wasm")) return "application/wasm";
            if (n.endsWith(".data")) return "application/octet-stream";
            if (n.endsWith(".css")) return "text/css";
            if (n.endsWith(".png")) return "image/png";
            if (n.endsWith(".jpg") || n.endsWith(".jpeg")) return "image/jpeg";
            if (n.endsWith(".svg")) return "image/svg+xml";
            if (n.endsWith(".json")) return "application/json";
            if (n.endsWith(".md")) return "text/plain";
            return "application/octet-stream";
        }
    }
}
