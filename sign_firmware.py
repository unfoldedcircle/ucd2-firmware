#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# ESP32 firmware binary signing with x509 private key.
# Based on https://forum.arduino.cc/t/signed-firmware-for-arduino-ota-x-509-rfc3161-working-code/649979
#
# Requires OpenSSL.
# Works as standalone tool or as post-build action in PlatformIO.
#
# Usage:
# ./sign_firmware.py --bin firmware.bin --out firmware.bin.signed [--privatekey secret-signing-key.pem] --model UCD2 --hw-rev 5.3
#
# The private key can also be specified in ENV variable OTA_SIGNING_KEY_FILE
#
import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

def get_hw_model():
    buildFlags = env.GetProjectOption("build_flags")
    for f in buildFlags:
        if "HW_MODEL=" in f:
            return f.partition("=")[2]
    sys.stderr.write("Missing HW_MODEL definition in PIO build_flag\n")
    sys.exit(1)

def get_hw_revision():
    buildFlags = env.GetProjectOption("build_flags")
    for f in buildFlags:
        if "HW_REVISION=" in f:
            return f.partition("=")[2]
    sys.stderr.write("Missing HW_REVISION definition in PIO build_flag\n")
    sys.exit(1)

def get_signing_key_file():
    # ENV variable must have priority because of CI build infrastructure!
    priv_key = os.getenv('OTA_SIGNING_KEY_FILE')
    if priv_key:
        return priv_key

    buildFlags = env.GetProjectOption("build_flags")
    for f in buildFlags:
        if "SIGNING_KEY_FILE=" in f:
            return f.partition("=")[2]

    sys.stderr.write("Missing SIGNING_KEY_FILE definition in PIO build_flag or OTA_SIGNING_KEY_FILE env variable\n")
    sys.exit(1)

def post_program_action(source, target, env):
    # check if signing is disabled, e.g. for local development build
    buildFlags = env.GetProjectOption("build_flags")
    for f in buildFlags:
        if "SKIP_SIGNING=" in f:
            if f.partition("=")[2] == "true":
                sys.stderr.write("WARNING: SKIP_SIGNING is set, skipping firmware signing!\n")
                return

    # either this is a bug or the documentation is wrong: https://docs.platformio.org/en/latest/scripting/actions.html
    program_path = source[0].get_abspath()
    #print("Post build action:", program_path)

    priv_key = get_signing_key_file()

    model = get_hw_model()
    revision = get_hw_revision()
    print('Got model: {} with revision {}'.format(model, revision))

    sign_and_write(program_path, priv_key, program_path + ".signed", model, revision)

try:
    Import("env")

    # Note only called IF the firmware.bin file is (re)created! Skipped if no build is required!
    env.AddPostAction("buildprog", post_program_action)
except Exception:
    pio = False # ignore, not called by PlatformIO


def parse_args():
    parser = argparse.ArgumentParser(description='Binary signing tool')
    parser.add_argument('-b', '--bin', help='Unsigned binary')
    parser.add_argument('-o', '--out', help='Output file')
    parser.add_argument('-s', '--privatekey', help='Private(secret) key file')
    parser.add_argument('-m', '--model', help='Dock model number')
    parser.add_argument('-r', '--hw-rev', help='Dock hardware revision number')
    return parser.parse_args()

def sign_and_write(in_file, priv_key, out_file, model, hw_rev):
    """Signs the in_file (file path) with the private key (file path)."""
    """Save the signed firmware to out_file (file path)."""

    rqh,rqn = tempfile.mkstemp(text=True)
    rph,rpn = tempfile.mkstemp(text=True)

    cmd = "openssl ts -query -data '%s' -cert -sha256 -no_nonce -out '%s'" % (in_file,rqn)
    print(cmd)
    if os.system(cmd):
        sys.exit(1)

    tmph,tmpn = tempfile.mkstemp(text=True)
    with os.fdopen(tmph,"w") as f:
      f.write("0000\n")
    tsch,tscn = tempfile.mkstemp(text=True)
    with os.fdopen(tsch,"w") as f:
      f.write("""
[ tsa ]
default_tsa = tsa_config

[ tsa_config ]
serial          = {}
crypto_device   = builtin
signer_digest   = sha256    
default_policy  = 1.2
other_policies  = 1.2
digests         = sha256
accuracy        = secs:1
ess_cert_id_alg	= sha1
ordering        = no
tsa_name        = no
ess_cert_id_chain = no

""".format(tmpn))

    cmd = "openssl ts -reply -queryfile '{}' -signer '{}' -inkey '{}' -out {} -config '{}'".format(rqn, priv_key, priv_key, rpn, tscn)
    print(cmd)
    if os.system(cmd):
        sys.exit(1)

    os.unlink(tmpn)
    os.unlink(tscn)

    f = open(rpn,'rb')
    tsr = f.read()
    f.close()

    os.unlink(rqn)
    os.unlink(rpn)

    content_size = os.path.getsize(in_file)

    len_tsr = 0 + len(tsr)
    hdr = 'RedWax/1.00 rfc3161=%d payload=%d model=%s hw=%s\n' % (len_tsr, content_size, model, hw_rev)
    content_size = content_size + len(hdr) + len_tsr

    with open(in_file, "rb") as b:
        data = b.read()

        md = hashlib.new('MD5')
        md.update(hdr.encode('ascii'))
        md.update(tsr)
        md.update(data)

        file_md = md.hexdigest()
        print('Upload size: {}, {}={}, header={}'.format(content_size, md.name, file_md, hdr), end='')

        with open(out_file, "wb") as out:
            out = open(out_file, "wb")
            out.write(hdr.encode('ascii'))
            out.write(tsr)
            out.write(data)

            print("Signed binary:", out_file)
        return 0

def main():
    args = parse_args()

    if not args.bin:
        sys.stderr.write("missing option --bin\n")
        return 1
    if not args.out:
        sys.stderr.write("missing option --out\n")
        return 1
    priv_key = os.getenv('OTA_SIGNING_KEY_FILE')
    if priv_key is None or not os.path.isfile(priv_key):
        if not args.privatekey:
            sys.stderr.write("missing option --privatekey or specify OTA_SIGNING_KEY_FILE environment variable\n")
            return 1
        priv_key = args.privatekey
    if not os.path.isfile(priv_key):
        sys.stderr.write("Private key file not found:", priv_key)
        return 1
    if not args.model:
        sys.stderr.write("missing option --model\n")
        return 1
    if not args.hw_rev:
        sys.stderr.write("missing option --hw-rev\n")
        return 1

    try:
        sign_and_write(args.bin, priv_key, args.out, args.model, args.hw_rev)

    except Exception as e:
        sys.stderr.write(str(e))
        sys.stderr.write("\nNot signing the generated binary\n")
        return 1

if __name__ == '__main__':
    sys.exit(main())
