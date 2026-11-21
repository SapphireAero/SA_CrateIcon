"""Build the standalone 32-bit Syringe plugin."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent


def main():
    env = os.environ.copy()
    toolchain = env.get("MOE_MINGW")
    if not toolchain and env.get("LOCALAPPDATA"):
        portable = Path(env["LOCALAPPDATA"]) / "MOEE-BuildTools" / "mingw32" / "bin"
        if (portable / "g++.exe").is_file():
            toolchain = str(portable)
    if toolchain:
        env["PATH"] = toolchain + os.pathsep + env.get("PATH", "")
    compiler = shutil.which("g++", path=env.get("PATH"))
    if not compiler:
        raise SystemExit("Set MOE_MINGW to the 32-bit MinGW-w64 bin directory.")
    target = subprocess.check_output([compiler, "-dumpmachine"], env=env, text=True).strip()
    if target != "i686-w64-mingw32":
        raise SystemExit(f"Expected i686-w64-mingw32, got {target}.")

    output = ROOT / "dist" / "SA_CrateIcon.dll"
    output.parent.mkdir(exist_ok=True)
    sources = sorted((ROOT / "src").glob("*.cpp"))
    with tempfile.TemporaryDirectory(prefix="sa-crateicon-") as temp:
        dll = str(Path(temp) / output.name)
        subprocess.run(
            [compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-fno-exceptions",
             "-fno-rtti", "-fno-threadsafe-statics", "-shared", "-static-libgcc", "-static-libstdc++",
             "-o", dll, *(str(source) for source in sources),
             "-Wl,--no-insert-timestamp", "-s"],
            cwd=ROOT, env=env, check=True,
        )
        shutil.copyfile(dll, output)
    print(f"Built {output}")


if __name__ == "__main__":
    main()
