#ifndef _H_RFC3161_UPDATE_PROCESSOR
#define _H_RFC3161_UPDATE_PROCESSOR

#include <Arduino.h>
#include <functional>
#include <mbedtls/md.h>
#include <mbedtls/x509.h>

#include "UpdateProcessor.h"

#define REDWAX_MAGIC_HEADER "RedWax/1."

class UpdateProcessorRFC3161 : public UpdateProcessor {
  public:
    UpdateProcessorRFC3161(UpdateProcessor  * chain = NULL);
    ~UpdateProcessorRFC3161();
    void reset();
    secure_update_processor_err_t process_header(uint32_t *command, uint8_t * buffer, size_t *len);
    secure_update_processor_err_t process_payload(uint8_t * buff, size_t *len);
    secure_update_processor_err_t process_end();

    /// @brief Parse one DER-encoded or one or more concatenated PEM-encoded certificates and add them to the chained list.
    /// @param len The size of \p buf, including the terminating \c NULL byte
    ///            in case of PEM encoded data.
    /// @param buf The buffer holding the certificate data in PEM or DER format.
    ///            For certificates in PEM encoding, this may be a concatenation
    ///            of multiple certificates; for DER encoding, the buffer must
    ///            comprise exactly one certificate.
    /// @return    \c 0 if all certificates were parsed successfully.
    ///            The (positive) number of certificates that couldn't
    ///            be parsed if parsing was partly successful (see above).
    ///            A negative X509 or PEM error code otherwise.
    int addTrustedCerts(const unsigned char * buf, size_t len);
    int setTrustedCerts(mbedtls_x509_crt * trustChain);
    int setAllowLegacyUploads(bool legacyAllowed);
  private:
    enum { INIT, RFC, POST } _state;
    unsigned char * _rfc3161;
    size_t _rfc3161_at;
    size_t _rfc3161_len;
    size_t _progress_flash;
    UpdateProcessor *_next;
    const mbedtls_md_info_t *_md_info;
    mbedtls_md_context_t * _md_ctx;
    size_t _payload_len = 0;
    mbedtls_ts_reply _reply;
    mbedtls_x509_crt * _trustChain;
    bool _legacyAllowed;
};
#endif
