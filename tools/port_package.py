"""Run python3 -I tools/port_package.py [--version NAME] [--ios | --android] to build the port with movies and pack it for
players into build/port/package: DC2.app in a zip on macOS, a folder with dc2 and its libraries in
a tar.gz on Linux, or a folder with DC2.exe and its DLLs in a zip on Windows, each beside the README, NOTICE,
LICENCE and THIRD-PARTY-LICENCES.md. Build the SDK first with
python3 -I tools/port_sdk.py. With --ios it packs an unsigned DC2.app for iPhone and iPad in an .ipa, with the
NOTICE and licences inside the app, from the SDKs python3 -I tools/ios_sdk.py builds. With --android it packs an arm64
APK, signed with port/android's public key and with the NOTICE and licences in its assets, from the SDKs
python3 -I tools/android_sdk.py builds."""

import argparse
import importlib.util
import math
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "port"
WINDOWS = sys.platform == "win32"
SDK = OUT / ("windows-sdk" if WINDOWS else "sdk")
IOS_SDK = OUT / "ios-sdk" / "device"
ANDROID_SDK = OUT / "android-sdk"
ANDROID = ROOT / "port" / "android"
ANDROID_MINIMUM = 29
ANDROID_TARGET = 35
KEY = {"store": ANDROID / "dc2.keystore", "alias": "dc2", "password": "dc2"}
PACKAGE = OUT / "package"
DARWIN = sys.platform == "darwin"
IDENTIFIER = "dc2toolkit.dc2"
MACOS_MINIMUM = "13.0"
ICON_SIZES = {"16x16": 16, "16x16@2x": 32, "32x32": 32, "32x32@2x": 64, "128x128": 128, "128x128@2x": 256,
              "256x256": 256, "256x256@2x": 512, "512x512": 512}


def version():
    return subprocess.check_output(["git", "describe", "--tags", "--always"], cwd=ROOT, text=True).strip()


def needed(binary):
    if DARWIN:
        listing = subprocess.check_output(["otool", "-L", str(binary)], text=True)
        return re.findall(r"^\s+@rpath/(\S+)", listing, re.M)
    listing = subprocess.check_output(["readelf", "-d", str(binary)], text=True)
    return re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", listing)


def copy_libraries(binary, folder):
    folder.mkdir(parents=True, exist_ok=True)
    pending = [binary]
    while pending:
        for name in needed(pending.pop()):
            source = SDK / "lib" / name
            target = folder / name
            if target.exists() or not source.exists():
                continue
            shutil.copyfile(source.resolve(), target)
            pending.append(target)


def stb_licence():
    text = (ROOT / "port/include/stb_truetype.h").read_text(encoding="utf-8")
    return text[text.index("ALTERNATIVE A - MIT License"):text.index("------", text.index("ALTERNATIVE A"))].strip()


def shared_ffmpeg():
    return ("GNU Lesser General Public License 2.1 or later. Its MPEG-2 decoder plays the game's movies, from the shared "
            "libraries libavcodec and libavutil shipped beside the program, which can be replaced with your own build. "
            + (SDK / "share/licenses/ffmpeg/BUILD.txt").read_text().strip())


def static_ffmpeg(tool):
    return ("GNU Lesser General Public License 2.1 or later. Its MPEG-2 decoder plays the game's movies. FFmpeg 8.0.1 "
            f"(https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz), unmodified, is built by tools/{tool} and linked "
            "into the app; the app's complete source, https://github.com/fakegenie/DC2-Toolkit, lets you relink it "
            "with your own build of FFmpeg.")


def licences(sdk, ffmpeg):
    sections = [
        ("SDL 3", "zlib licence. https://libsdl.org", (sdk / "share/licenses/SDL3/LICENSE.txt").read_text()),
        ("FFmpeg", ffmpeg, (sdk / "share/licenses/ffmpeg/COPYING.LGPLv2.1").read_text()),
        ("Manrope", "SIL Open Font License 1.1, the launcher's font. https://github.com/sharanda/manrope",
         (ROOT / "port/launcher/OFL.txt").read_text()),
        ("stb_truetype", "MIT licence or public domain, which draws the launcher's text. https://github.com/nothings/stb",
         stb_licence()),
        ("Kazarov's libvu0", "MIT licence, from ChronicleTwo's Kazarov toolchain.",
         (ROOT / "engine/tools/kazarov/LICENSE").read_text()),
    ]
    lines = ["# Third-party licences", "", "The DC2 port contains or ships with the following."]
    for name, about, text in sections:
        lines += ["", f"## {name}", "", about, "", "```", text.strip(), "```"]
    return "\n".join(lines) + "\n"


def notices(folder, sdk, ffmpeg):
    shutil.copyfile(ROOT / "port/package/NOTICE.md", folder / "NOTICE.md")
    shutil.copyfile(ROOT / "LICENCE", folder / "LICENCE")
    (folder / "THIRD-PARTY-LICENCES.md").write_text(licences(sdk, ffmpeg), encoding="utf-8")


def icon(resources):
    iconset = OUT / "icon.iconset"
    shutil.rmtree(iconset, ignore_errors=True)
    iconset.mkdir()
    for name, pixels in ICON_SIZES.items():
        subprocess.run(["sips", "-z", str(pixels), str(pixels), str(ROOT / "port/launcher/icon.png"),
                        "--out", str(iconset / f"icon_{name}.png")], check=True, capture_output=True)
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(resources / "icon.icns")], check=True)
    shutil.rmtree(iconset)


def bundle_version(name):
    found = re.match(r"v?(\d+(?:\.\d+)*)", name)
    return found.group(1) if found else "0"


def app_info(name):
    return {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": "DC2",
        "CFBundleExecutable": "DC2",
        "CFBundleIconFile": "icon",
        "CFBundleIdentifier": IDENTIFIER,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "DC2",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": bundle_version(name),
        "CFBundleVersion": bundle_version(name),
        "LSApplicationCategoryType": "public.app-category.role-playing-games",
        "LSMinimumSystemVersion": MACOS_MINIMUM,
        "NSHighResolutionCapable": True,
    }


def macos(name):
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/port.py"), "--movies"], check=True)
    folder = PACKAGE / f"dc2-{name}-macos-arm64"
    folder.mkdir(parents=True)
    shutil.copyfile(ROOT / "port/package/README.md", folder / "README.md")
    notices(folder, SDK, shared_ffmpeg())
    contents = folder / "DC2.app" / "Contents"
    executable = contents / "MacOS" / "DC2"
    executable.parent.mkdir(parents=True)
    shutil.copyfile(OUT / "dc2", executable)
    executable.chmod(0o755)
    subprocess.run(["strip", str(executable)], check=True)
    copy_libraries(executable, contents / "Frameworks")
    (contents / "Resources").mkdir()
    icon(contents / "Resources")
    (contents / "Info.plist").write_bytes(plistlib.dumps(app_info(name)))
    for library in sorted((contents / "Frameworks").iterdir()):
        subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(library)], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(contents.parent)], check=True)
    archive = PACKAGE / f"{folder.name}.zip"
    subprocess.run(["ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", str(folder), str(archive)], check=True)
    return archive


def linux(name):
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/port.py"), "--movies"], check=True)
    folder = PACKAGE / f"dc2-{name}-linux-x64"
    folder.mkdir(parents=True)
    shutil.copyfile(ROOT / "port/package/README.md", folder / "README.md")
    notices(folder, SDK, shared_ffmpeg())
    executable = folder / "dc2"
    shutil.copyfile(OUT / "dc2", executable)
    executable.chmod(0o755)
    subprocess.run(["strip", str(executable)], check=True)
    copy_libraries(executable, folder / "lib")
    for library in (folder / "lib").iterdir():
        subprocess.run(["strip", "--strip-unneeded", str(library)], check=True)
    archive = PACKAGE / f"{folder.name}.tar.gz"
    subprocess.run(["tar", "--owner=0", "--group=0", "--numeric-owner", "-czf", str(archive), "-C", str(PACKAGE),
                    folder.name], check=True)
    return archive


def windows(name):
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/port.py"), "--movies"], check=True)
    built = OUT / "windows"
    strip = OUT / "llvm-mingw" / "bin" / "llvm-strip.exe"
    folder = PACKAGE / f"dc2-{name}-windows-x64"
    folder.mkdir(parents=True)
    shutil.copyfile(ROOT / "port/package/README.md", folder / "README.md")
    notices(folder, SDK, shared_ffmpeg())
    executable = folder / "DC2.exe"
    shutil.copyfile(built / "dc2.exe", executable)
    subprocess.run([str(strip), str(executable)], check=True)
    for library in sorted(built.glob("*.dll")):
        shutil.copyfile(library, folder / library.name)
    archive = PACKAGE / f"{folder.name}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as packed:
        for path in sorted(folder.rglob("*")):
            packed.write(path, path.relative_to(PACKAGE).as_posix())
    return archive


def ios(name):
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/port.py"), "--ios", "device"], check=True)
    payload = PACKAGE / "Payload"
    app = payload / "DC2.app"
    shutil.copytree(OUT / "ios-device" / "DC2.app", app)
    subprocess.run(["xcrun", "strip", str(app / "DC2")], check=True)
    info = plistlib.loads((app / "Info.plist").read_bytes())
    info["CFBundleShortVersionString"] = info["CFBundleVersion"] = bundle_version(name)
    (app / "Info.plist").write_bytes(plistlib.dumps(info, fmt=plistlib.FMT_BINARY))
    notices(app, IOS_SDK, static_ffmpeg("ios_sdk.py"))
    archive = PACKAGE / f"dc2-{name}-ios.ipa"
    subprocess.run(["ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", str(payload), str(archive)], check=True)
    shutil.rmtree(payload)
    return archive


def android_home():
    spec = importlib.util.spec_from_file_location("android_sdk", Path(__file__).with_name("android_sdk.py"))
    android_sdk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(android_sdk)
    return android_sdk.sdk_root(), android_sdk.toolchain()


def newest(folder):
    return max(folder.iterdir(), key=lambda path: [int(part) for part in re.findall(r"\d+", path.name)])


def version_code(name):
    parts = [int(part) for part in bundle_version(name).split(".")][:3]
    parts += [0] * (3 - len(parts))
    return max(1, parts[0] * 1000000 + parts[1] * 1000 + parts[2])


ANDROID_DENSITIES = [("mdpi", 48, 108), ("hdpi", 72, 162), ("xhdpi", 96, 216), ("xxhdpi", 144, 324), ("xxxhdpi", 192, 432)]
ANDROID_ADAPTIVE_ICON = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/icon_background" />
    <foreground android:drawable="@mipmap/icon_foreground" />
</adaptive-icon>
"""


def read_png_rgba(path):
    data = path.read_bytes()
    pos, width, height, color_type, idat = 8, 0, 0, 0, bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, _, color_type = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat.extend(body)
    raw = zlib.decompress(bytes(idat))
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    out, prev, index = bytearray(width * height * 4), bytearray(stride), 0
    for y in range(height):
        flt = raw[index]
        row = bytearray(raw[index + 1:index + 1 + stride])
        index += 1 + stride
        if flt == 1:
            for x in range(channels, stride):
                row[x] = (row[x] + row[x - channels]) & 0xFF
        elif flt == 2:
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 0xFF
        elif flt == 3:
            for x in range(stride):
                a = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif flt == 4:
            for x in range(stride):
                a = row[x - channels] if x >= channels else 0
                b, c = prev[x], prev[x - channels] if x >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        prev = row
        if channels == 4:
            out[y * width * 4:(y + 1) * width * 4] = row
        else:
            for x in range(width):
                out[(y * width + x) * 4:(y * width + x) * 4 + 4] = bytes([row[x * 3], row[x * 3 + 1], row[x * 3 + 2], 255])
    return width, height, out


def write_png_rgba(path, width, height, rgba):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(rgba[y * width * 4:(y + 1) * width * 4])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def box_scale_rgba(src, sw, sh, dw, dh):
    out = bytearray(dw * dh * 4)
    for dy in range(dh):
        y0, y1 = dy * sh / dh, (dy + 1) * sh / dh
        iy0, iy1 = int(y0), min(sh - 1, int(math.ceil(y1)) - 1)
        for dx in range(dw):
            x0, x1 = dx * sw / dw, (dx + 1) * sw / dw
            ix0, ix1 = int(x0), min(sw - 1, int(math.ceil(x1)) - 1)
            tr = tg = tb = tw = 0.0
            for sy in range(iy0, iy1 + 1):
                wy = min(y1, sy + 1.0) - max(y0, float(sy))
                if wy <= 0:
                    continue
                for sx in range(ix0, ix1 + 1):
                    wx = min(x1, sx + 1.0) - max(x0, float(sx))
                    if wx <= 0:
                        continue
                    w = wx * wy
                    idx = (sy * sw + sx) * 4
                    tr += src[idx] * w
                    tg += src[idx + 1] * w
                    tb += src[idx + 2] * w
                    tw += w
            di = (dy * dw + dx) * 4
            out[di:di + 4] = bytes([min(255, int(tr / tw + 0.5)), min(255, int(tg / tw + 0.5)), min(255, int(tb / tw + 0.5)), 255])
    return out


def android_icons(res):
    iw, ih, idata = read_png_rgba(ROOT / "port/launcher/icon-ios.png")
    fg432 = bytearray(432 * 432 * 4)
    start, span = 16.0 / 108.0, 76.0 / 108.0

    def mirror(v, limit):
        while v < 0 or v >= limit:
            v = -v if v < 0 else 2 * limit - 1.001 - v
        return v

    for py in range(432):
        sy = mirror(((py + 0.5) / 432 - start) / span * ih - 0.5, ih - 1)
        iy, fy = int(sy), sy - int(sy)
        for px in range(432):
            sx = mirror(((px + 0.5) / 432 - start) / span * iw - 0.5, iw - 1)
            ix, fx = int(sx), sx - int(sx)
            rgb = []
            for ch in range(3):
                p00, p10 = idata[(iy * iw + ix) * 4 + ch], idata[(iy * iw + ix + 1) * 4 + ch]
                p01, p11 = idata[((iy + 1) * iw + ix) * 4 + ch], idata[((iy + 1) * iw + ix + 1) * 4 + ch]
                rgb.append(int((p00 * (1 - fx) + p10 * fx) * (1 - fy) + (p01 * (1 - fx) + p11 * fx) * fy + 0.5))
            fg432[(py * 432 + px) * 4:(py * 432 + px) * 4 + 4] = bytes([rgb[0], rgb[1], rgb[2], 255])

    viewport = bytearray(288 * 288 * 4)
    for y in range(288):
        viewport[y * 288 * 4:(y + 1) * 288 * 4] = fg432[((72 + y) * 432 + 72) * 4:((72 + y) * 432 + 360) * 4]

    values = res / "values"
    values.mkdir(parents=True, exist_ok=True)
    (values / "colors.xml").write_text('<?xml version="1.0" encoding="utf-8"?>\n<resources><color name="icon_background">#1B8AB8</color></resources>\n', encoding="utf-8")
    anydpi = res / "mipmap-anydpi-v26"
    anydpi.mkdir(parents=True, exist_ok=True)
    (anydpi / "icon.xml").write_text(ANDROID_ADAPTIVE_ICON, encoding="utf-8")
    (anydpi / "icon_round.xml").write_text(ANDROID_ADAPTIVE_ICON, encoding="utf-8")

    for density, legacy_px, adaptive_px in ANDROID_DENSITIES:
        folder = res / f"mipmap-{density}"
        write_png_rgba(folder / "icon_foreground.png", adaptive_px, adaptive_px, fg432 if adaptive_px == 432 else box_scale_rgba(fg432, 432, 432, adaptive_px, adaptive_px))
        scaled = box_scale_rgba(viewport, 288, 288, legacy_px, legacy_px)
        circled, rounded = bytearray(scaled), bytearray(scaled)
        half = legacy_px / 2.0
        for y in range(legacy_px):
            dy = abs((y + 0.5) - half)
            for x in range(legacy_px):
                dx = abs((x + 0.5) - half)
                cdist = math.hypot(dx, dy) - (half - 0.5)
                circled[(y * legacy_px + x) * 4 + 3] = int(min(max(0.5 - cdist, 0.0), 1.0) * 255.0 + 0.5)
                inner, corner = legacy_px * 14.0 / 64.0, legacy_px * 12.0 / 64.0
                qx, qy = max(dx - inner, 0.0), max(dy - inner, 0.0)
                rdist = math.hypot(qx, qy) + min(max(dx - inner, dy - inner), 0.0) - corner
                rounded[(y * legacy_px + x) * 4 + 3] = int(min(max(0.5 - rdist, 0.0), 1.0) * 255.0 + 0.5)
        write_png_rgba(folder / "icon_round.png", legacy_px, legacy_px, circled)
        write_png_rgba(folder / "icon.png", legacy_px, legacy_px, rounded)


def android(name):
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/port.py"), "--android"], check=True)
    home, toolchain = android_home()
    tools = newest(home / "build-tools")
    platform = str(newest(home / "platforms") / "android.jar")
    work = OUT / "apk"
    shutil.rmtree(work, ignore_errors=True)
    sources = sorted([*(ANDROID_SDK / "java").rglob("*.java"), *(ANDROID / "java").rglob("*.java")])
    subprocess.run(["javac", "--release", "17", "-nowarn", "-Xlint:none", "-classpath", platform, "-d", str(work / "classes"),
                    *map(str, sources)], check=True)
    subprocess.run([str(tools / "d8"), "--release", "--min-api", str(ANDROID_MINIMUM), "--lib", platform, "--output",
                    str(work), *map(str, sorted((work / "classes").rglob("*.class")))], check=True)
    android_icons(work / "res")
    subprocess.run([str(tools / "aapt2"), "compile", "--dir", str(work / "res"), "-o", str(work / "res.zip")], check=True)
    assets = work / "assets"
    assets.mkdir()
    notices(assets, ANDROID_SDK, static_ffmpeg("android_sdk.py"))
    packed = work / "packed.apk"
    subprocess.run([str(tools / "aapt2"), "link", "-o", str(packed), "-I", platform, "--manifest",
                    str(ANDROID / "AndroidManifest.xml"), "--min-sdk-version", str(ANDROID_MINIMUM),
                    "--target-sdk-version", str(ANDROID_TARGET), "--version-code", str(version_code(name)),
                    "--version-name", bundle_version(name), "-A", str(assets), str(work / "res.zip")], check=True)
    shutil.copyfile(OUT / "android" / "libmain.so", work / "libmain.so")
    subprocess.run([str(toolchain / "bin" / "llvm-strip"), "--strip-unneeded", str(work / "libmain.so")], check=True)
    with zipfile.ZipFile(packed, "a") as apk:
        apk.write(work / "classes.dex", "classes.dex", compress_type=zipfile.ZIP_DEFLATED)
        for library in [ANDROID_SDK / "lib" / "libSDL3.so", work / "libmain.so"]:
            apk.write(library, f"lib/arm64-v8a/{library.name}", compress_type=zipfile.ZIP_STORED)
    aligned = work / "aligned.apk"
    subprocess.run([str(tools / "zipalign"), "-P", "16", "-f", "4", str(packed), str(aligned)], check=True)
    archive = PACKAGE / f"dc2-{name}-android-arm64.apk"
    subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(KEY["store"]), "--ks-key-alias", KEY["alias"],
                    "--ks-pass", f"pass:{KEY['password']}", "--out", str(archive), str(aligned)], check=True)
    archive.with_name(archive.name + ".idsig").unlink(missing_ok=True)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", default=None, help="the release's name, git describe's by default")
    parser.add_argument("--ios", action="store_true", help="pack an unsigned .ipa for iPhone and iPad")
    parser.add_argument("--android", action="store_true", help="pack an arm64 APK")
    options = parser.parse_args()
    if options.ios and not (IOS_SDK / "lib/libSDL3.a").exists():
        sys.exit("iOS SDL3 and FFmpeg missing: run python3 -I tools/ios_sdk.py")
    if options.android and not (ANDROID_SDK / "lib/libSDL3.so").exists():
        sys.exit("Android SDL3 and FFmpeg missing: run python3 -I tools/android_sdk.py")
    if not options.ios and not options.android and not (SDK / "include/SDL3/SDL.h").exists():
        sys.exit("SDL3 and FFmpeg missing: run python3 -I tools/port_sdk.py")
    name = options.version or version()
    shutil.rmtree(PACKAGE, ignore_errors=True)
    PACKAGE.mkdir(parents=True)
    archive = (ios(name) if options.ios else android(name) if options.android else macos(name) if DARWIN
               else windows(name) if WINDOWS else linux(name))
    print(f"packed {archive.relative_to(ROOT)} ({os.path.getsize(archive) >> 20} MB)")


if __name__ == "__main__":
    main()
