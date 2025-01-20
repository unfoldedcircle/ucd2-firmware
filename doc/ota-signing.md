# Signed OTA-Images

This document describes the concept behind the signed OTA-images. See source code and build scripts for the actual implementation.

Firmware images are signed with x509 certificates, issued by our own PKI. We are using a dedicated PKI for the Dock OTA
images, which is not used for other certificates. In the future, this could easily be changed if required, by simply
exchanging the public certificates embedded on the device.

The firmware image doesn't contain a file system. All required public certificates for image validation are embedded in
the code during the build process.  
Note: certificates are only used for OTA verification. The WebSocket API server is http only, and there are no outgoing
connections.

## Public Key Infrastructure

```
 Foobar Inc root CA
   |
   +---- Production
   |
   +---- Engineering
             |
             +---- Development
             |         |
             |     (optional)
             |         |
             |         +- Dev 1
             |         +- Dev 2
             |         +- Dev 3
             |
             +---- Testing
```

### Production vs Development separation concept

- Docks running a production firmware are not allowed to be flashed with a development firmware.
- Docks running a development firmware may be flashed with a production firmware.

This verification is done with the embedded public keys chain, which is different for the two firmware images:
- Production firmware includes the `production` -> `root` public keys.
- Development firmware includes the `development` -> `engineering` -> `root` public keys.

Certificate validation requires the parent intermediate certificate:
- A production signed OTA firmware image requires the `root` public key.
  - Since this public key is included in both firmware images, a production firmware can be flashed over a development
    and a production firmware.
- A development signed OTA firmware image requires the `engineering` public key.
  - Since this public key is only included in the development firmware image, a production firmware cannot be flashed
    over a development firmware.

⚠️ Attention: this is a very simple concept, and a firmware signed with the `engineering` key, can be flashed over a
production image.

A proper implementation would use certificate policies. It's "good enough" for our use case 😊

### Create x509 certificates

#### Root CA

```bash
openssl req -x509 \
  -subj "/O=$COMPANY/OU=$PRODUCT_CA/CN=UCD2 OTA root CA" \
  -days 3660 \
  -keyout 'ucd2-ota-root.key' -out 'ucd2-ota-root.crt' \
  -nodes -sha256 \
  -addext 'extendedKeyUsage=critical,timeStamping'

cat ucd2-ota-root.key > ucd2-ota-root-signing-key.pem
cat ucd2-ota-root.crt >> ucd2-ota-root-signing-key.pem
```

⚠️ make sure to set a long expiration date!  
For our use case, 10 years gives enough time to rotate keys.

#### Intermediate CA

Create a configuration file for the basic constraints and timestamping key usage, `tsa.cnf`:
```
basicConstraints=critical,CA:true
extendedKeyUsage=critical,timeStamping
```

We have the following intermediate CAs (see above):

- `Production` and `Engineering` below the root CA
  - need to be signed with the parent _root CA_: `ucd2-ota-root-signing-key.pem`
- `Development` and `Testing` below `Engineering`
  - need to be signed with the parent _Engineering CA_: `engineering-signing-key.pem`

Create private key and a signing request:

```shell
export INTERMEDIATE=production
mkdir -p $INTERMEDIATE
cd $INTERMEDIATE

openssl genpkey -algorithm RSA \
    -out $INTERMEDIATE.key
openssl req -new -key $INTERMEDIATE.key -sha256 \
    -subj "/O=$COMPANY/OU=$PRODUCT_CA/CN=$INTERMEDIATE" \
    -out $INTERMEDIATE.csr
```

Sign it with the parent CA:
```shell
export PARENT=../ucd2-ota-root
export DAYS=3660

openssl x509 -req -in $INTERMEDIATE.csr \
    -CA $PARENT-signing-key.pem -CAkey $PARENT-signing-key.pem \
    -CAcreateserial \
    -extfile $INTERMEDIATE.cnf \
    -days $DAYS -sha256 \
    -out $INTERMEDIATE.crt

cat $INTERMEDIATE.key > $INTERMEDIATE-signing-key.pem
cat $INTERMEDIATE.crt >> $INTERMEDIATE-signing-key.pem
```

Repeat for `engineering` and `engineering/development`. Adjust `DAYS` depending on environment:
- engineering: 360
- development: 90

### Show x509 certificate

```bash
openssl x509 -in $INTERMEDIATE-signing-key.pem -noout -text
```

Show certificate purpose:
```bash
openssl x509 -noout -in $INTERMEDIATE-signing-key.pem -purpose
```

### Extract public key in DER format to embed in firmware

```bash
openssl x509 -in $INTERMEDIATE-signing-key.pem -outform DER -out ota_public_key.der
```

## Sign firmware image

Built firmware images are automatically signed with a post build action in PIO.

```bash
./sign_firmware.py --bin .pio/build/UCD2-r5_3/firmware.bin \
    --out .pio/build/UCD2-r5_3/firmware.bin.signed \
    --privatekey $INTERMEDIATE-signing-key.pem \
    --model UCD2 \
    --hw-rev 5.3
```

## Upload

```bash
curl  -F "file=@.pio/build/UCD2-r5_3/firmware.bin.signed" http://UC-Dock-5443B2D65A5C.local/update
```

## Implementation

Based on: https://github.com/dirkx/arduino-esp32/tree/arduino-signed-updater/libraries/Update

More information:

- [Digital Signing of Over The Air (OTA) updates.](../lib/Update/digital-signing.md)
- <https://forum.arduino.cc/t/signed-firmware-for-arduino-ota-x-509-rfc3161-working-code/649979>


### Embedded public key

The public key is embedded in the code with the `COMPONENT_EMBED_FILES` build flag in [platform.ini](../platformio.ini):
```
build_flags =
  ${flags:runtime.build_flags}
  -DCOMPONENT_EMBED_FILES=ota_public_key.der ; public key in DER format for OTA image verification
```

Source code: [service_ota.cpp](../lib/service_ota/service_ota.cpp)
```cpp
extern const uint8_t ota_public_key_der_start[] asm("_binary_ota_public_key_der_start");
extern const uint8_t ota_public_key_der_end[] asm("_binary_ota_public_key_der_end");
```

The embedded DER encoded key is directly passed to [mbedtls_x509_crt_init](https://github.com/Mbed-TLS/mbedtls/blob/4ebe2a737273667125c5723d38189f871257f3f4/include/mbedtls/x509_crt.h#LL871C34-L871C34) in [UpdateProcessorRFC3161::addTrustedCertAsDER](../lib/Update/src/UpdateProcessorRFC3161.cpp).
The [mbedtls_x509_crt structure](https://github.com/Mbed-TLS/mbedtls/blob/4ebe2a737273667125c5723d38189f871257f3f4/include/mbedtls/x509_crt.h#L54-L103) allows a certificate chain:
```
    /** Next certificate in the linked list that constitutes the CA chain.
     * \p NULL indicates the end of the list.
     * Do not modify this field directly. */
```

Certificate chain initialization:
- With [mbedtls_x509_crt_parse](https://os.mbed.com/teams/sandbox/code/mbedtls/docs/tip/group__x509__module.html#ga033567483649030f7f859db4f4cb7e14)
  - Either one DER-encoded or one or more concatenated PEM-encoded certificates.
- https://forums.mbed.com/t/verify-certificate-chain/4533

⚠️ If embedding PEM-encoded certificates one has to use the `COMPONENT_EMBED_TXTFILES` build flag!  
Otherwise it's missing the \0 byte termination at the end, which the `mbedtls_x509_crt_parse` function expects and
therefore fails to parse the certificate (chain).

### Binary OTA image format

The OTA image is built with [sign_firmware.py](../sign_firmware.py) in function `sign_and_write`, which is called from the PIO build process. See [platform.ini](../platformio.ini):
```ini
extra_scripts = 
  pre:build_firmware_version.py
  pre:build_set_hw_revision.py
  post:sign_firmware.py
```

The signed image starts with an ASCII header:
- Start text: `RedWax/A.BB`, where A=1 and B can be any digits
- It may be followed by key-value pairs. This header line is ended by a '\n' (0x0A) and can not be longer than 128 bytes (\n included).

For version 1.XX it is immediately followed by a DER encoded RFC3161 timestamp/signature 'reply'.
And this is followed by the original 'raw' firmware image.

```
+----------------------------------------------------------------+
| Header (terminated with \n, max lenght: 128 bytes)             |
| `RedWax/1.00 rfc3161=%d payload=%d model=%s hw=%s\n`           |
+----------------------------------------------------------------+
| DER encoded RFC3161 timestamp/signature 'reply'                |
| (length is specified in header field `rfc3161`)                |
+----------------------------------------------------------------+
| Binary ESP32 firmware image                                    |
| (starting with magic byte 0xE7 / SP_IMAGE_HEADER_MAGIC)        |
|                                                                |
|                                                                |
+----------------------------------------------------------------+
```

Example header: `header=RedWax/1.00 rfc3161=1511 payload=1784688 model=UCD2 hw=5.4`

- Signature size: 1511
- Firmware image size: 1784688
- Dock model number: UCD2
- Hardware revision: 5.4
