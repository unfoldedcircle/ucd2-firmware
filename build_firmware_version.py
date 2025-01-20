import os
import subprocess

Import("env")

def get_firmware_specifier_build_flag():
    # If run with a GitHub action, the version is available in ENV variable FW_VERSION
    # Use this version, since the git repository might be dirty from replacing a signing key!
    build_version = os.getenv('FW_VERSION')
    if build_version:
        print ("Using build version from FW_VERSION")
    else:
        ret = subprocess.run(["./tools/git-version.sh"], stdout=subprocess.PIPE, text=True)
        build_version = ret.stdout.strip()

    build_flag = "-D DOCK_VERSION=\\\"" + build_version + "\\\""
    print ("Firmware revision: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)
