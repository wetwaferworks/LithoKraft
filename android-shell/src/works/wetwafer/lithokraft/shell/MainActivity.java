package works.wetwafer.lithokraft.shell;

import android.app.Activity;
import android.net.http.SslError;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.webkit.ConsoleMessage;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.webkit.Profile;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.HashSet;
import java.util.Set;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLServerSocketFactory;

/**
 * LithoKraft Shell — a test shell that boots the shipped LithoKraft web
 * build full-screen in the platform WebView (Android's embedded Chromium).
 *
 * The web package lives in APK assets and is served by a tiny in-app
 * HTTPS server bound to 127.0.0.1 (see LocalHttpsServer below), so the
 * WebView loads it from a genuine https://127.0.0.1 origin with the
 * cross-origin-isolation headers (COOP/COEP/CORP) the WASM build's
 * threads require.
 *
 * Why not the v1.0–v1.3 approach (shouldInterceptRequest answering
 * https://app.local/ from assets)? Device evidence, 2026-10-20 (moto g
 * 5G - 2024, WebView 155.0.8059.43): the page loaded, but the served
 * context was NOT cross-origin isolated — `SharedArrayBuffer is not
 * defined` killed the thin layer's boot at its RPC slot, even though
 * the isolation headers were set on every intercepted response. WebView
 * does not run its isolation processing for intercepted responses;
 * the headers never took effect. A real HTTP response over a real
 * socket goes through Chromium's network stack, which does. (The thin
 * layer no longer hard-depends on the standalone SharedArrayBuffer
 * constructor either — its RPC slot now lives in the shared wasm
 * memory — but worker creation and any future SAB use still want a
 * truly isolated origin, and localhost is the honest way to get one.)
 *
 * Why HTTPS in v1.5 (the hardware↔CEF seam, JD's placement)? v1.4
 * served the same bundle over plain http://127.0.0.1 with the full
 * isolation header set, and the device boot log read
 * crossOriginIsolated=false: the headers were proven present and
 * correct (workstation audit, 2026-10-20 — the same discipline grants
 * isolation in desktop Chromium), so the refusal is a property of the
 * origin class in WebView 155, not of the headers. October 2026
 * platform reporting describes WebView's isolation grant as scoped to
 * approved HTTPS origins, which an http origin can never be. So the
 * shell moved its origin up a class: a self-signed loopback
 * certificate (generated at build time, bundled as an asset) and TLS
 * on the same loopback server. The certificate error WebView raises
 * for that self-signed cert is proceeded past ONLY for this app's own
 * loopback host (see onReceivedSslError) — a test-shell measure,
 * stated plainly: this shell is JD's on-device test rig, not a
 * distribution build, and the loopback TLS key ships inside it.
 *
 * v1.6 — the public-certificate experiment (JD's direction, 2026-10-21:
 * "grab those public certificates, implement the HTTPS layer"). The
 * v1.5 device verdict was refusal again (crossOriginIsolated=false on
 * the self-signed HTTPS origin), leaving the self-signed certificate
 * itself — loaded only by proceeding past its SSL error — as the prime
 * suspect for disqualifying the origin. So v1.6 holds everything else
 * constant and changes ONE variable: the certificate. If the asset
 * `lk-shell-cert.p12` is bundled (a chain-valid, publicly trusted
 * Let's Encrypt certificate for PUBLIC_HOST, issued via DNS-01 — see
 * cert/issue-shell-cert.sh), the shell serves the identical bundle
 * from https://PUBLIC_HOST:<port>/, where PUBLIC_HOST's public DNS
 * A record points at 127.0.0.1, so the phone resolves the name to its
 * own loopback server while TLS validates against a public root with
 * NO SSL error and NO proceed crutch (onReceivedSslError cancels for
 * PUBLIC_HOST: an error there would be genuine). Without that asset
 * the shell falls back to the v1.5 loopback profile unchanged. JD's
 * framing: the hardware layer negotiates with public certificates,
 * and platforms are certified at launch — this profile is that
 * negotiation, tested. Test rig caveat, stated plainly: the LE key
 * for a disposable test subdomain ships inside this test APK, exactly
 * like the loopback key did; a distribution build would not do this.
 *
 * v1.7 — the cross-origin-isolated ALLOWLIST attempt (JD dispatch,
 * 2026-10-21: "try and rebuild it and see if we can get this right
 * one last time"). Every origin class tried so far (intercepted,
 * http loopback, https loopback self-signed, appassets) was refused
 * by WebView 155. AndroidX WebKit 1.18 added the platform-sanctioned
 * variable not yet tried: Profile#setCrossOriginIsolatedAllowlist
 * (feature WebViewFeature.CROSS_ORIGIN_ISOLATED_ALLOWLIST) — the app
 * names exact origins where cross-origin-isolated APIs become
 * accessible, PROVIDED the page sends the correct
 * Document-Isolation-Policy response header (the server now sends
 * `Document-Isolation-Policy: isolate-and-require-corp` on every
 * response, alongside COOP/COEP/CORP). Before loadUrl, this activity
 * allowlists this app's own server origins (loopback and public
 * profiles, exact host:port). All wiring is feature-gated and
 * failure-soft: unsupported or failing degrades to exactly v1.6
 * behavior. The device remains the only real gate for the grant;
 * onPageFinished reports the feature state, the allowlist readback,
 * and the page's own isolation values into the page log on every
 * boot, diag mode or not, so a screenshot carries the verdict.
 */
public class MainActivity extends Activity {

    private static final String TAG = "LithoKraftShell";

    // v1.6 public-cert profile: hostname whose public A record points
    // at 127.0.0.1, with a chain-valid Let's Encrypt certificate
    // bundled as PUBLIC_KS_ASSET when issued (cert/issue-shell-cert.sh;
    // default domain matches that script's defaults — if JD issues
    // for the other domain, this constant and the script must agree).
    static final String PUBLIC_HOST = "shell.lithocraft.net";
    private static final String PUBLIC_KS_ASSET = "lk-shell-cert.p12";
    private static final String LOOPBACK_KS_ASSET = "lk-localhost.p12";

    private WebView webView;
    private boolean diagMode;
    private boolean publicProfile;
    private LocalHttpsServer server;
    // v1.7 allowlist wiring outcome, read by the onPageFinished
    // instrumentation so every boot reports it into the page log.
    private boolean coiFeatureSupported;
    private String coiAllowlistReadback = "(not attempted)";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Keep the screen on while the game is up.
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

        webView.setWebViewClient(new LocalAssetClient());
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage msg) {
                Log.i(TAG, msg.message() + " (" + msg.sourceId() + ":" + msg.lineNumber() + ")");
                return true;
            }
        });

        // Diagnostic builds carry an empty asset named lk-diag.flag: the
        // pages then boot with ?diag=1 (present-path instrumentation in
        // the on-screen log — black-screen diagnosis, 2026-10-20). The
        // flag also makes the shell identify its WebView build and the
        // page's isolation state in the log.
        diagMode = assetExists("lk-diag.flag");
        // v1.6 profile selection: the public-cert profile engages only
        // when its issued keystore was bundled at build time; otherwise
        // the shell is v1.5 exactly (loopback origin, self-signed cert).
        publicProfile = assetExists(PUBLIC_KS_ASSET);
        try {
            server = new LocalHttpsServer(publicProfile ? PUBLIC_KS_ASSET : LOOPBACK_KS_ASSET);
            server.start();
        } catch (Exception e) {
            Log.e(TAG, "local server failed to start: " + e.getMessage());
            throw new RuntimeException("LithoKraft shell: localhost server failed", e);
        }
        // v1.7: allowlist wiring, after the server is up (the port is
        // known) and before the first load. Failure-soft by design.
        applyCrossOriginIsolatedAllowlist();
        String host = publicProfile ? PUBLIC_HOST : "127.0.0.1";
        String entry = "https://" + host + ":" + server.port() + "/gl.html" + (diagMode ? "?diag=1" : "");
        Log.i(TAG, "entry: " + entry + " (diag=" + diagMode
                + ", profile=" + (publicProfile ? "public-cert" : "loopback") + ")");
        webView.loadUrl(entry);
    }

    /**
     * v1.7 — name this app's own server origins on the WebView
     * profile's cross-origin-isolated allowlist (AndroidX WebKit
     * 1.18, Profile#setCrossOriginIsolatedAllowlist). Exact origins
     * only — never a wildcard. Paired with the
     * Document-Isolation-Policy header the local server sends on
     * every response. Any unsupported feature or failure leaves the
     * shell behaving exactly as v1.6; nothing here may crash the
     * activity.
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
            origins.add("https://127.0.0.1:" + server.port());
            origins.add("https://" + PUBLIC_HOST + ":" + server.port());
            profile.setCrossOriginIsolatedAllowlist(origins);
            try {
                coiAllowlistReadback = String.valueOf(profile.getCrossOriginIsolatedAllowlist());
            } catch (Throwable t) {
                coiAllowlistReadback = "(readback failed: " + t.getMessage() + ")";
            }
            Log.i(TAG, "coi allowlist: set " + origins + "; readback " + coiAllowlistReadback);
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
        if (server != null) {
            server.stop();
            server = null;
        }
        super.onDestroy();
    }

    private class LocalAssetClient extends WebViewClient {

        @Override
        public void onPageFinished(WebView view, String url) {
            super.onPageFinished(view, url);
            // v1.7 field instrumentation — runs on EVERY boot, diag
            // mode or not: one [shell] line in the page log carrying
            // the allowlist wiring outcome and the page's own
            // isolation verdict, so a screenshot settles the attempt.
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
            // Identify the exact WebView build in the page's own log: the
            // present path's behavior is a property of this build.
            String pkg = "(unknown)";
            try {
                android.content.pm.PackageInfo pi = WebView.getCurrentWebViewPackage();
                if (pi != null) pkg = pi.packageName + " " + pi.versionName;
            } catch (Exception e) {
                pkg = "(unavailable: " + e.getMessage() + ")";
            }
            String ua = view.getSettings().getUserAgentString();
            String line = "[shell] diag shell; webview package: " + pkg + "; shell ua: " + ua;
            Log.i(TAG, line);
            // And the isolation probe (shell v1.4): the exact properties
            // whose absence killed the v1.3 boot on device, printed into
            // the page log where a screenshot can carry them home.
            String probe = "[shell] isolation probe runs in-page (see next line)";
            String js = "(function(){var el=document.getElementById('log');if(!el)return;"
                    + "el.textContent += " + jsString(line) + " + '\\n';"
                    + "var p='[shell] crossOriginIsolated=' + window.crossOriginIsolated"
                    + " + ' SharedArrayBuffer=' + (typeof SharedArrayBuffer)"
                    + " + ' Worker=' + (typeof Worker)"
                    + " + ' origin=' + location.origin;"
                    + "el.textContent += p + '\\n';})()";
            Log.i(TAG, probe);
            view.evaluateJavascript(js, null);
        }

        private String jsString(String s) {
            return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'";
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            // Keep navigation inside the shell; block anything off-origin.
            String host = request.getUrl().getHost();
            return !("127.0.0.1".equals(host) || "localhost".equals(host) || PUBLIC_HOST.equals(host));
        }

        @Override
        public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
            // SSL-error policy by host (v1.6):
            // - PUBLIC_HOST: CANCEL, always. Its certificate is
            //   chain-valid by construction (public CA, matching name);
            //   an error there means the wrong certificate is being
            //   served or a genuine attack is in progress — there is
            //   nothing legitimate to proceed past, and proceeding
            //   would destroy the experiment's one changed variable.
            // - Loopback hosts: TEST-SHELL MEASURE, stated plainly
            //   (see the class header): the only TLS endpoint there is
            //   the app's own loopback server, whose certificate is
            //   the self-signed one generated at build time and
            //   bundled in assets — WebView cannot chain it to a
            //   trusted root, so every load of the loopback origin
            //   raises an error here. Proceed ONLY for it, so the
            //   proceed can never bless a real site or a real attack.
            // - Anything else: cancel.
            String url = error.getUrl();
            String host = null;
            try { host = android.net.Uri.parse(url).getHost(); } catch (Exception ignored) {}
            if (PUBLIC_HOST.equals(host)) {
                Log.w(TAG, "ssl: CANCELLING load with ssl error for public-cert host (must be chain-valid): " + url);
                handler.cancel();
            } else if ("127.0.0.1".equals(host) || "localhost".equals(host)) {
                Log.i(TAG, "ssl: proceeding for own loopback origin (" + url + ")");
                handler.proceed();
            } else {
                Log.w(TAG, "ssl: CANCELLING load with ssl error for non-loopback url: " + url);
                handler.cancel();
            }
        }
    }

    /**
     * Minimal HTTPS/1.1 server for the bundled web package: loopback
     * only, ephemeral port, thread per connection, GET/HEAD only. Every
     * response (including 404s) carries the cross-origin-isolation
     * header set. Hand-written, no dependencies — same rule as the rest
     * of this shell's build. TLS comes from the keystore asset named at
     * construction: the build-time self-signed loopback certificate
     * (`lk-localhost.p12`, see build.sh) for the loopback profile, or
     * the issued chain-valid Let's Encrypt certificate
     * (`lk-shell-cert.p12`, see cert/issue-shell-cert.sh) for the
     * public-cert profile — the server binds 127.0.0.1 either way; in
     * the public profile the phone simply reaches it under a public
     * name. The keystore password is the shell's own debug-grade
     * constant below — the shell is a test rig (see class header).
     */
    private class LocalHttpsServer {
        private final String ksAsset;
        // Instance field, not static: a non-static inner class cannot
        // declare a static non-constant field (javac, source 8).
        private final char[] KS_PASS = "lithokraft".toCharArray();
        private SSLServerSocket socket;
        private Thread acceptThread;
        private volatile boolean running;

        LocalHttpsServer(String ksAsset) {
            this.ksAsset = ksAsset;
        }

        private SSLServerSocketFactory tlsFactory() throws Exception {
            KeyStore ks = KeyStore.getInstance("PKCS12");
            try (InputStream in = getAssets().open(ksAsset)) {
                ks.load(in, KS_PASS);
            }
            KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
            kmf.init(ks, KS_PASS);
            SSLContext ctx = SSLContext.getInstance("TLS");
            ctx.init(kmf.getKeyManagers(), null, null);
            return ctx.getServerSocketFactory();
        }

        void start() throws Exception {
            socket = (SSLServerSocket) tlsFactory().createServerSocket(0, 50, InetAddress.getByName("127.0.0.1"));
            running = true;
            // No lambdas in this file: javac against the android.jar
            // bootclasspath cannot resolve LambdaMetafactory.
            acceptThread = new Thread(new Runnable() {
                @Override public void run() {
                    while (running) {
                        try {
                            final Socket conn = socket.accept();
                            Thread t = new Thread(new Runnable() {
                                @Override public void run() { handle(conn); }
                            });
                            t.setDaemon(true);
                            t.start();
                        } catch (IOException e) {
                            if (running) Log.w(TAG, "accept failed: " + e.getMessage());
                        }
                    }
                }
            }, "lk-https-accept");
            acceptThread.setDaemon(true);
            acceptThread.start();
            Log.i(TAG, "local server on 127.0.0.1:" + socket.getLocalPort());
        }

        int port() {
            return socket.getLocalPort();
        }

        void stop() {
            running = false;
            try { socket.close(); } catch (IOException ignored) {}
        }

        private void handle(Socket conn) {
            try (Socket c = conn) {
                c.setSoTimeout(30000);
                BufferedReader in = new BufferedReader(
                        new InputStreamReader(c.getInputStream(), StandardCharsets.ISO_8859_1));
                String requestLine = in.readLine();
                if (requestLine == null) return;
                // Consume the remaining headers; the shell needs none of them.
                String h;
                while ((h = in.readLine()) != null && !h.isEmpty()) { /* discard */ }
                String[] parts = requestLine.split(" ");
                if (parts.length < 2) { send(c, 400, "Bad Request", "text/plain", new byte[0], false); return; }
                String method = parts[0];
                String path = parts[1];
                int q = path.indexOf('?');
                if (q >= 0) path = path.substring(0, q);
                path = percentDecode(path);
                if (path == null || path.isEmpty() || "/".equals(path)) path = "/gl.html";
                if (path.contains("..")) { send(c, 403, "Forbidden", "text/plain", new byte[0], "HEAD".equals(method)); return; }
                String assetName = path.startsWith("/") ? path.substring(1) : path;
                byte[] body;
                try (InputStream ain = getAssets().open(assetName)) {
                    body = readAll(ain);
                } catch (IOException e) {
                    Log.w(TAG, "asset miss: " + assetName);
                    send(c, 404, "Not Found", "text/plain", new byte[0], "HEAD".equals(method));
                    return;
                }
                if (!"GET".equals(method) && !"HEAD".equals(method)) {
                    send(c, 405, "Method Not Allowed", "text/plain", new byte[0], false);
                    return;
                }
                send(c, 200, "OK", mimeTypeFor(assetName), body, "HEAD".equals(method));
            } catch (IOException e) {
                Log.w(TAG, "connection failed: " + e.getMessage());
            }
        }

        private void send(Socket c, int code, String reason, String mime, byte[] body, boolean headOnly)
                throws IOException {
            StringBuilder sb = new StringBuilder();
            sb.append("HTTP/1.1 ").append(code).append(' ').append(reason).append("\r\n");
            sb.append("Content-Type: ").append(mime);
            if (mime.startsWith("text/") || mime.equals("application/json")) sb.append("; charset=utf-8");
            sb.append("\r\n");
            sb.append("Content-Length: ").append(body.length).append("\r\n");
            // Cross-origin isolation: honored here because this is a real
            // HTTP response through Chromium's network stack (the v1.3
            // intercepted responses never got this processing in WebView).
            sb.append("Cross-Origin-Opener-Policy: same-origin\r\n");
            sb.append("Cross-Origin-Embedder-Policy: require-corp\r\n");
            sb.append("Cross-Origin-Resource-Policy: same-origin\r\n");
            // v1.7: the header the profile allowlist mechanism
            // requires the page to send (see the class header).
            sb.append("Document-Isolation-Policy: isolate-and-require-corp\r\n");
            sb.append("Cache-Control: no-store\r\n");
            sb.append("Connection: close\r\n");
            sb.append("\r\n");
            OutputStream out = c.getOutputStream();
            out.write(sb.toString().getBytes(StandardCharsets.ISO_8859_1));
            if (!headOnly && body.length > 0) out.write(body);
            out.flush();
        }

        private byte[] readAll(InputStream in) throws IOException {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            byte[] chunk = new byte[65536];
            int n;
            while ((n = in.read(chunk)) >= 0) buf.write(chunk, 0, n);
            return buf.toByteArray();
        }

        private String percentDecode(String s) {
            StringBuilder out = new StringBuilder(s.length());
            for (int i = 0; i < s.length(); i++) {
                char ch = s.charAt(i);
                if (ch == '%' && i + 2 < s.length()) {
                    int hi = Character.digit(s.charAt(i + 1), 16);
                    int lo = Character.digit(s.charAt(i + 2), 16);
                    if (hi >= 0 && lo >= 0) {
                        out.append((char) ((hi << 4) + lo));
                        i += 2;
                        continue;
                    }
                }
                out.append(ch);
            }
            return out.toString();
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
