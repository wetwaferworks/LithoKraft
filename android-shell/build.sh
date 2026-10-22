#!/bin/sh
# Hand-rolled APK build for LithoKraft Shell — javac + aapt2 + d8 + zipalign + apksigner.
# No Gradle. One library dependency since v1.6: androidx.webkit (Leg B,
# WebViewAssetLoader), vendored as an AAR under libs/ and pinned by
# SHA-256 below; everything else is the Android SDK build-tools.
# v1.7 bumps the vendored AAR to 1.18.0-rc01 for the Profile
# cross-origin-isolated allowlist API (see MainActivity's header).
set -e
cd "$(dirname "$0")"

TOOLCHAIN="${ANDROID_TOOLCHAIN:-$HOME/workspace/android-toolchain}"
JDK="$TOOLCHAIN/jdk"
SDK="$TOOLCHAIN/android-sdk"
BT="$SDK/build-tools/34.0.0"
ANDROID_JAR="$SDK/platforms/android-34/android.jar"
export PATH="$JDK/bin:$PATH"

KS="lithokraft-shell-debug.keystore"
KS_PASS="lithokraft"

rm -rf build
mkdir -p build/classes build/dex

echo "== loopback TLS certificate (shell v1.5) =="
# The shell serves its bundle over https://127.0.0.1 (the origin class
# WebView's cross-origin-isolation grant accepts; plain http got
# crossOriginIsolated=false on device, WebView 155, with correct
# headers). The certificate is self-signed, generated once, and
# bundled as an asset; the app proceeds past its (expected) cert
# error for the loopback host only. Test shell only — see
# MainActivity's header comment.
if [ ! -f assets/lk-localhost.p12 ]; then
    keytool -genkeypair -keystore assets/lk-localhost.p12 -storetype PKCS12 \
        -alias localhost -keyalg RSA -keysize 2048 -validity 3650 \
        -storepass lithokraft -keypass lithokraft \
        -dname "CN=localhost, O=Wet Wafer Works" \
        -ext "san=dns:localhost,ip:127.0.0.1"
fi
ls -la assets/lk-localhost.p12

echo "== androidx.webkit (Leg B: WebViewAssetLoader; v1.7: Profile allowlist) =="
WEBKIT_AAR="libs/webkit-1.18.0-rc01.aar"
WEBKIT_AAR_SHA256="5474b66ccabad971df70b8d42318caa2937652a71b329813386ca72eb0526250"
if [ ! -f "$WEBKIT_AAR" ]; then
    mkdir -p libs
    curl -sL -o "$WEBKIT_AAR" \
        "https://dl.google.com/dl/android/maven2/androidx/webkit/webkit/1.18.0-rc01/webkit-1.18.0-rc01.aar"
fi
echo "$WEBKIT_AAR_SHA256  $WEBKIT_AAR" | sha256sum -c -
mkdir -p build/webkit
unzip -o -q "$WEBKIT_AAR" classes.jar -d build/webkit
WEBKIT_JAR="build/webkit/classes.jar"

echo "== public-cert profile (v1.6, Leg A) =="
# If cert/issue-shell-cert.sh has issued a chain-valid certificate, it
# lands at assets/lk-shell-cert.p12 and MainActivity selects the
# public-cert profile at runtime. Absent = the v1.5 loopback profile.
if [ -f assets/lk-shell-cert.p12 ]; then
    echo "lk-shell-cert.p12 PRESENT — public-cert profile will engage (host: shell.lithocraft.net)"
    ls -la assets/lk-shell-cert.p12
else
    echo "lk-shell-cert.p12 absent — loopback profile (v1.5 behavior); see cert/JD-CLICK-LIST.md"
fi

echo "== javac =="
javac -encoding UTF-8 -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -cp "$WEBKIT_JAR" -d build/classes \
    src/works/wetwafer/lithokraft/shell/MainActivity.java \
    src/works/wetwafer/lithokraft/shell/AssetsActivity.java

echo "== jar + d8 =="
(cd build/classes && jar cf ../classes.jar .)
# v1.7: build-tools 34.0.0's d8 (R8 8.2.2-dev) dies with an internal
# NullPointerException on androidx.webkit 1.18's classes.jar
# (androidx/webkit/internal/JavaScriptReplyProxyImpl$1.class). The
# cmdline-tools D8 (R8 8.2.33) dexes the same input cleanly, so
# prefer it when present; fall back to the build-tools d8 otherwise.
R8_JAR="$SDK/cmdline-tools/latest/lib/r8.jar"
if [ -f "$R8_JAR" ]; then
    "$JDK/bin/java" -cp "$R8_JAR" com.android.tools.r8.D8 \
        --lib "$ANDROID_JAR" --min-api 26 --output build/dex build/classes.jar "$WEBKIT_JAR"
else
    "$BT/d8" --lib "$ANDROID_JAR" --min-api 26 --output build/dex build/classes.jar "$WEBKIT_JAR"
fi

echo "== aapt2 compile (res: network security config) =="
"$BT/aapt2" compile --dir res -o build/res.zip

echo "== aapt2 link (manifest + res + assets) =="
"$BT/aapt2" link -o build/app-unsigned.apk \
    -I "$ANDROID_JAR" \
    --manifest AndroidManifest.xml \
    build/res.zip \
    -A assets

echo "== add classes.dex =="
(cd build/dex && jar uf ../app-unsigned.apk classes.dex)

echo "== zipalign =="
"$BT/zipalign" -f -p 4 build/app-unsigned.apk build/app-aligned.apk

echo "== keystore (debug, project-local) =="
if [ ! -f "$KS" ]; then
    keytool -genkeypair -keystore "$KS" -alias lithokraft-shell \
        -keyalg RSA -keysize 2048 -validity 10950 \
        -storepass "$KS_PASS" -keypass "$KS_PASS" \
        -dname "CN=LithoKraft Shell Debug, O=Wet Wafer Works"
fi

echo "== apksigner sign =="
"$BT/apksigner" sign --ks "$KS" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" \
    --out build/LithoKraftShell.apk build/app-aligned.apk

echo "== verify =="
"$BT/apksigner" verify --print-certs build/LithoKraftShell.apk
"$BT/aapt2" dump badging build/LithoKraftShell.apk | head -4
ls -la build/LithoKraftShell.apk
sha256sum build/LithoKraftShell.apk
echo "BUILD OK -> build/LithoKraftShell.apk"
