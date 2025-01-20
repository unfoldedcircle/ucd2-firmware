# Add a C pre-processor flag from the HW_REVISION build flag.
# Creates a new flag with the value as suffix, and dot replaced by underscore.
# E.g. HW_REVISION=42.5 will create: HW_REVISION_42_5
# See board.h where this is used to import the corresponding board definition file.
Import("env")

def get_hw_revision():
    buildFlags = env.GetProjectOption("build_flags")
    for f in buildFlags:
        if "HW_REVISION=" in f:
            return f.partition("=")[2]
    return ""

revision = get_hw_revision()
flag = "HW_REVISION_" + revision.replace(".", "_")
print("HW revision:", revision)
print("Build flag :", flag)

env.Append(
    BUILD_FLAGS=["-D " + flag]
)
