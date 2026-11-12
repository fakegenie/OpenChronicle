import io, zipfile, zlib, sys

def array(name, data):
    lines = []
    for start in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[start:start + 16]) + ",")
    return "static const uint8_t %s[%d] = {\n%s\n};\n" % (name, len(data), "\n".join(lines))

text = b"The quick brown fox jumps over the lazy dog. " * 20
even = bytes(range(0, 32))
odd = bytes(0x80 + i for i in range(32))
buffer = io.BytesIO()
with zipfile.ZipFile(buffer, "w") as archive:
    archive.writestr(zipfile.ZipInfo("game/Hello.TXT"), text, compress_type=zipfile.ZIP_DEFLATED)
    archive.writestr(zipfile.ZipInfo("even.bin"), even, compress_type=zipfile.ZIP_STORED)
    archive.writestr(zipfile.ZipInfo("odd.bin"), odd, compress_type=zipfile.ZIP_DEFLATED)

pattern = bytes((i * i + i // 7) % 251 for i in range(4000))
dynamic = zlib.compressobj(9, zlib.DEFLATED, -15)
packed = dynamic.compress(pattern) + dynamic.flush()
stored = zlib.compressobj(0, zlib.DEFLATED, -15)
plain = stored.compress(pattern[:300]) + stored.flush()

sys.stdout.write(array("fixture_zip", buffer.getvalue()))
sys.stdout.write(array("fixture_dynamic", packed))
sys.stdout.write(array("fixture_stored", plain))
sys.stdout.write("static const uint32_t fixture_hello_crc = 0x%08Xu;\n" % zlib.crc32(text))
sys.stdout.write("static const uint32_t fixture_odd_crc = 0x%08Xu;\n" % zlib.crc32(odd))
