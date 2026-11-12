import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = "shaders/crt.frag"
BYTES_PER_LINE = 16


def compile_shader(folder):
    spirv = Path(folder) / "crt.spv"
    msl = Path(folder) / "crt.msl"
    subprocess.run(["glslangValidator", "-V", "-S", "frag", "-o", str(spirv), SOURCE], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["spirv-cross", "--msl", "--output", str(msl), str(spirv)], check=True)
    return spirv.read_bytes(), msl.read_bytes()


def c_array(name, data):
    lines = [", ".join("0x%02x" % byte for byte in data[start:start + BYTES_PER_LINE]) for start in range(0, len(data), BYTES_PER_LINE)]
    body = "".join("    %s,\n" % line for line in lines)
    return "static const unsigned char %s[] = {\n%s};\n" % (name, body)


def main():
    with tempfile.TemporaryDirectory() as folder:
        spirv, msl = compile_shader(folder)
    sys.stdout.write("#ifndef PH_CRT_SHADER_H\n#define PH_CRT_SHADER_H\n\n")
    sys.stdout.write(c_array("ph_crt_spirv", spirv))
    sys.stdout.write("\n")
    sys.stdout.write(c_array("ph_crt_msl", msl))
    sys.stdout.write("\n#endif\n")


if __name__ == "__main__":
    main()
