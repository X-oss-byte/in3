import os
import json
import sys
import subprocess
import pytest
import shutil


class CMake:
    def __init__(self, factory):
        self.runs = {}
        self.factory = factory

    def compile(self, targets, options=None):
        if options is None:
            options = {}
        key = (
            ";".join(targets),
            ";".join(f"{k}={v}" for k, v in options.items()),
        )

        if key not in self.runs:
            cwd = self.factory.mktemp("cmake")
            self.runs[key] = cwd
            cmake(cwd, targets, options)

        return self.runs[key]

    def destroy(self):
        sourcedir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
        coveragedir = os.path.join(sourcedir, "coverage")
        shutil.rmtree(coveragedir, ignore_errors=True)
        os.mkdir(coveragedir)

        if "llvm-cov" in os.environ.get("RUN_ANALYZER", ""):
            for i, d in enumerate(self.runs.values()):
                # first merge the raw profiling runs
                files = [f for f in os.listdir(d) if f.endswith(".profraw")]
                if not files:
                    continue
                cmd = [
                    "llvm-profdata",
                    "merge",
                    "-sparse",
                    "-o=sentry.profdata",
                    *files,
                ]
                print(f'{d} > {" ".join(cmd)}')
                subprocess.run(cmd, cwd=d)

                # then export lcov from the profiling data, since this needs access
                # to the object files, we need to do it per-test
                objects = [
                    "sentry_example",
                    "sentry_test_unit",
                    "libsentry.dylib" if sys.platform == "darwin" else "libsentry.so",
                ]
                cmd = [
                    "llvm-cov",
                    "export",
                    "-format=lcov",
                    "-instr-profile=sentry.profdata",
                    "--ignore-filename-regex=(external|vendor|tests|examples)",
                    *[f"-object={o}" for o in objects if d.joinpath(o).exists()],
                ]
                lcov = os.path.join(coveragedir, f"run-{i}.lcov")
                with open(lcov, "w") as lcov_file:
                    print(f'{d} > {" ".join(cmd)} > {lcov}')
                    subprocess.run(cmd, stdout=lcov_file, cwd=d)

        if "kcov" in os.environ.get("RUN_ANALYZER", ""):
            if coverage_dirs := [
                d
                for d in [d.joinpath("coverage") for d in self.runs.values()]
                if d.exists()
            ]:
                subprocess.run(
                    ["kcov", "--clean", "--merge", coveragedir, *coverage_dirs,]
                )


def cmake(cwd, targets, options=None):
    __tracebackhide__ = True
    if options is None:
        options = {}
    options.update(
        {
            "CMAKE_RUNTIME_OUTPUT_DIRECTORY": cwd,
            "CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG": cwd,
            "CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE": cwd,
        }
    )
    if os.environ.get("ANDROID_API") and os.environ.get("ANDROID_NDK"):
        # See: https://developer.android.com/ndk/guides/cmake
        toolchain = f'{os.environ["ANDROID_HOME"]}/ndk/{os.environ["ANDROID_NDK"]}/build/cmake/android.toolchain.cmake'
        options.update(
            {
                "CMAKE_TOOLCHAIN_FILE": toolchain,
                "ANDROID_ABI": os.environ.get("ANDROID_ARCH") or "x86",
                "ANDROID_NATIVE_API_LEVEL": os.environ["ANDROID_API"],
            }
        )

    source_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    cmake = ["cmake"]
    if "scan-build" in os.environ.get("RUN_ANALYZER", ""):
        cc = os.environ.get("CC")
        cxx = os.environ.get("CXX")
        cmake = [
            "scan-build",
            *(["--use-cc", cc] if cc else []),
            *(["--use-c++", cxx] if cxx else []),
            "--status-bugs",
            "--exclude",
            os.path.join(source_dir, "external"),
            "cmake",
        ]

    configcmd = cmake.copy()
    configcmd.extend(f"-D{key}={value}" for key, value in options.items())
    if sys.platform == "win32" and os.environ.get("TEST_X86"):
        configcmd.append("-AWin32")
    elif sys.platform == "linux" and os.environ.get("TEST_X86"):
        configcmd.append("-DSENTRY_BUILD_FORCE32=ON")
    if "asan" in os.environ.get("RUN_ANALYZER", ""):
        configcmd.append("-DWITH_ASAN_OPTION=ON")

    configcmd.append(source_dir)

    # we have to set `-Werror` for this cmake invocation only, otherwise
    # completely unrelated things will break
    cflags = []
    if os.environ.get("ERROR_ON_WARNINGS"):
        cflags.append("-Werror")
    if sys.platform == "win32":
        # MP = object level parallelism, WX = warnings as errors
        cpus = os.cpu_count()
        cflags.append(f"/WX /MP{cpus}")
    if "gcc" in os.environ.get("RUN_ANALYZER", ""):
        cflags.append("-fanalyzer")
    if "llvm-cov" in os.environ.get("RUN_ANALYZER", ""):
        cflags.append("-fprofile-instr-generate -fcoverage-mapping")
    env = dict(os.environ)
    env["CFLAGS"] = env["CXXFLAGS"] = " ".join(cflags)

    print(f'\n{cwd} > {" ".join(configcmd)}', flush=True)
    try:
        subprocess.run(configcmd, cwd=cwd, env=env, check=True)
    except subprocess.CalledProcessError:
        raise pytest.fail.Exception("cmake configure failed") from None

    # CodeChecker invocations and options are documented here:
    # https://github.com/Ericsson/codechecker/blob/master/docs/analyzer/user_guide.md

    buildcmd = [*cmake, "--build", ".", "--parallel"]
    for target in targets:
        buildcmd.extend(["--target", target])
    if "code-checker" in os.environ.get("RUN_ANALYZER", ""):
        buildcmd = [
            "CodeChecker",
            "log",
            "--output",
            "compilation.json",
            "--build",
            " ".join(buildcmd),
        ]

    print(f'{cwd} > {" ".join(buildcmd)}', flush=True)
    try:
        subprocess.run(buildcmd, cwd=cwd, check=True)
    except subprocess.CalledProcessError:
        raise pytest.fail.Exception("cmake build failed") from None

    if "code-checker" in os.environ.get("RUN_ANALYZER", ""):
        # For whatever reason, the compilation summary contains duplicate entries,
        # one with the correct absolute path, and the other one just with the basename,
        # which would fail.
        with open(os.path.join(cwd, "compilation.json")) as f:
            compilation = json.load(f)
            compilation = list(filter(lambda c: c["file"].startswith("/"), compilation))
        with open(os.path.join(cwd, "compilation.json"), "w") as f:
            json.dump(compilation, f)

        disable = [
            "readability-magic-numbers",
            "cppcoreguidelines-avoid-magic-numbers",
            "readability-else-after-return",
        ]
        disables = [f"--disable={d}" for d in disable]
        checkcmd = [
            "CodeChecker",
            "check",
            "--jobs",
            str(os.cpu_count()),
            # NOTE: The clang version on CI does not support CTU :-(
            # Also, when testing locally, CTU spews a ton of (possibly) false positives
            # "--ctu-all",
            # TODO: we currently get >300 reports with `enable-all`
            # "--enable-all",
            *disables,
            "--print-steps",
            "--ignore",
            os.path.join(source_dir, ".codechecker-ignore"),
            "--logfile",
            "compilation.json",
        ]
        print(f'{cwd} > {" ".join(checkcmd)}', flush=True)
        child = subprocess.run(checkcmd, stdout=subprocess.PIPE, cwd=cwd, check=True)
        sys.stdout.buffer.write(child.stdout)
        marker = b"Total number of reports: "
        errors = child.stdout[child.stdout.rfind(marker) + len(marker) :]
        errors = int(errors[: errors.find(b"\n")])
        if errors > 0:
            pytest.fail("code-checker analysis failed")

    if os.environ.get("ANDROID_API"):
        # copy the output to the android image via adb
        subprocess.run(
            [
                f'{os.environ["ANDROID_HOME"]}/platform-tools/adb',
                "push",
                "./",
                "/data/local/tmp",
            ],
            cwd=cwd,
            check=True,
        )
