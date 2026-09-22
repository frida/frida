#ifdef HAVE_NICE

#include "frida-base.h"

#include <errno.h>
#include <usrsctp.h>

#ifdef HAVE_GIOAPPLE
# include <CoreFoundation/CoreFoundation.h>
# include <Security/Security.h>
#else
# define OPENSSL_SUPPRESS_DEPRECATED
# include <openssl/asn1.h>
# include <openssl/bio.h>
# include <openssl/bn.h>
# include <openssl/evp.h>
# include <openssl/pem.h>
# include <openssl/rsa.h>
# include <openssl/x509.h>
#endif

#ifdef HAVE_GIOAPPLE
static SecKeyRef frida_rsa_keypair_generate (void);
static void frida_rsa_export_public_key (SecKeyRef private_key, GByteArray * out);
static void frida_rsa_export_private_key (SecKeyRef private_key, GByteArray * out);
static void frida_rsa_sign_sha256 (SecKeyRef private_key, const guint8 * message, gsize message_length, GByteArray * out);

static void frida_build_tbs_certificate (GByteArray * out, const GByteArray * subject_public_key);
static void frida_build_name (GByteArray * out);
static void frida_build_rdn (GByteArray * out, const guint8 * oid, gsize oid_length, guint8 value_tag, const gchar * value);
static void frida_build_validity (GByteArray * out);
static void frida_build_subject_public_key_info (GByteArray * out, const GByteArray * rsa_public_key);
static void frida_build_signed_certificate (GByteArray * out, const GByteArray * tbs, const GByteArray * signature);
static void frida_build_algorithm_identifier (GByteArray * out, const guint8 * oid, gsize oid_length);
static void frida_build_pkcs8_private_key_info (GByteArray * out, const GByteArray * rsa_private_key);
static gchar * frida_pem_encode (const gchar * type, const guint8 * der, gsize der_length);

static void frida_der_append_tlv (GByteArray * out, guint8 tag, const guint8 * value, gsize length);
static void frida_der_append_length (GByteArray * out, gsize length);
static void frida_der_append_integer_uint (GByteArray * out, guint64 value);
static void frida_der_append_null (GByteArray * out);
static void frida_der_append_oid (GByteArray * out, const guint8 * oid, gsize length);
static void frida_der_append_utctime (GByteArray * out, time_t t);
static void frida_der_append_bit_string (GByteArray * out, const guint8 * bytes, gsize length);

static const guint8 frida_oid_country[] = { 0x55, 0x04, 0x06 };
static const guint8 frida_oid_organization[] = { 0x55, 0x04, 0x0a };
static const guint8 frida_oid_common_name[] = { 0x55, 0x04, 0x03 };
static const guint8 frida_oid_rsa_encryption[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };
static const guint8 frida_oid_sha256_with_rsa[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b };
#else
static gchar * frida_steal_bio_to_string (BIO ** bio);
#endif

static int frida_on_connection_output (void * addr, void * buffer, size_t length, uint8_t tos, uint8_t set_df);
static void frida_on_debug_printf (const char * format, ...);

static void frida_on_upcall (struct socket * sock, void * user_data, int flags);

#ifdef HAVE_GIOAPPLE

void
_frida_generate_certificate (guint8 ** cert_der, gint * cert_der_length, gchar ** cert_pem, gchar ** key_pem)
{
  SecKeyRef private_key;
  GByteArray * rsa_public_key, * rsa_private_key, * subject_public_key, * tbs, * signature, * certificate, * pkcs8;

  *cert_der = NULL;
  *cert_der_length = 0;
  *cert_pem = NULL;
  *key_pem = NULL;

  private_key = frida_rsa_keypair_generate ();

  rsa_public_key = g_byte_array_new ();
  frida_rsa_export_public_key (private_key, rsa_public_key);

  rsa_private_key = g_byte_array_new ();
  frida_rsa_export_private_key (private_key, rsa_private_key);

  subject_public_key = g_byte_array_new ();
  frida_build_subject_public_key_info (subject_public_key, rsa_public_key);

  tbs = g_byte_array_new ();
  frida_build_tbs_certificate (tbs, subject_public_key);

  signature = g_byte_array_new ();
  frida_rsa_sign_sha256 (private_key, tbs->data, tbs->len, signature);

  certificate = g_byte_array_new ();
  frida_build_signed_certificate (certificate, tbs, signature);

  pkcs8 = g_byte_array_new ();
  frida_build_pkcs8_private_key_info (pkcs8, rsa_private_key);

  *cert_der = g_memdup2 (certificate->data, certificate->len);
  *cert_der_length = certificate->len;
  *cert_pem = frida_pem_encode ("CERTIFICATE", certificate->data, certificate->len);
  *key_pem = frida_pem_encode ("PRIVATE KEY", pkcs8->data, pkcs8->len);

  g_byte_array_unref (pkcs8);
  g_byte_array_unref (certificate);
  g_byte_array_unref (signature);
  g_byte_array_unref (tbs);
  g_byte_array_unref (subject_public_key);
  g_byte_array_unref (rsa_private_key);
  g_byte_array_unref (rsa_public_key);
  CFRelease (private_key);
}

static SecKeyRef
frida_rsa_keypair_generate (void)
{
  SecKeyRef private_key;
  CFMutableDictionaryRef attrs;
  const int key_size_bits = 2048;
  CFNumberRef key_size;

  attrs = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

  key_size = CFNumberCreate (kCFAllocatorDefault, kCFNumberIntType, &key_size_bits);
  CFDictionarySetValue (attrs, kSecAttrKeyType, kSecAttrKeyTypeRSA);
  CFDictionarySetValue (attrs, kSecAttrKeySizeInBits, key_size);
  CFRelease (key_size);

  private_key = SecKeyCreateRandomKey (attrs, NULL);
  g_assert (private_key != NULL);

  CFRelease (attrs);

  return private_key;
}

static void
frida_rsa_export_public_key (SecKeyRef private_key, GByteArray * out)
{
  SecKeyRef public_key;
  CFDataRef data;

  public_key = SecKeyCopyPublicKey (private_key);
  g_assert (public_key != NULL);

  data = SecKeyCopyExternalRepresentation (public_key, NULL);
  g_assert (data != NULL);

  g_byte_array_append (out, CFDataGetBytePtr (data), CFDataGetLength (data));

  CFRelease (data);
  CFRelease (public_key);
}

static void
frida_rsa_export_private_key (SecKeyRef private_key, GByteArray * out)
{
  CFDataRef data;

  data = SecKeyCopyExternalRepresentation (private_key, NULL);
  g_assert (data != NULL);

  g_byte_array_append (out, CFDataGetBytePtr (data), (guint) CFDataGetLength (data));

  CFRelease (data);
}

static void
frida_rsa_sign_sha256 (SecKeyRef private_key, const guint8 * message, gsize message_length, GByteArray * out)
{
  CFDataRef to_sign, signature;

  to_sign = CFDataCreate (kCFAllocatorDefault, message, (CFIndex) message_length);

  signature = SecKeyCreateSignature (private_key, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, to_sign, NULL);
  g_assert (signature != NULL);

  g_byte_array_append (out, CFDataGetBytePtr (signature), (guint) CFDataGetLength (signature));

  CFRelease (signature);
  CFRelease (to_sign);
}

static void
frida_build_tbs_certificate (GByteArray * out, const GByteArray * subject_public_key)
{
  GByteArray * inner = g_byte_array_new ();

  frida_der_append_integer_uint (inner, 1);
  frida_build_algorithm_identifier (inner, frida_oid_sha256_with_rsa, sizeof (frida_oid_sha256_with_rsa));
  frida_build_name (inner);
  frida_build_validity (inner);
  frida_build_name (inner);
  g_byte_array_append (inner, subject_public_key->data, subject_public_key->len);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_name (GByteArray * out)
{
  GByteArray * inner = g_byte_array_new ();

  frida_build_rdn (inner, frida_oid_country, sizeof (frida_oid_country), 0x13, "CA");
  frida_build_rdn (inner, frida_oid_organization, sizeof (frida_oid_organization), 0x0c, "Frida");
  frida_build_rdn (inner, frida_oid_common_name, sizeof (frida_oid_common_name), 0x0c, "lolcathost");

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_rdn (GByteArray * out, const guint8 * oid, gsize oid_length, guint8 value_tag, const gchar * value)
{
  GByteArray * atv, * rdn;

  atv = g_byte_array_new ();
  rdn = g_byte_array_new ();

  frida_der_append_oid (atv, oid, oid_length);
  frida_der_append_tlv (atv, value_tag, (const guint8 *) value, strlen (value));
  frida_der_append_tlv (rdn, 0x30, atv->data, atv->len);
  frida_der_append_tlv (out, 0x31, rdn->data, rdn->len);

  g_byte_array_unref (rdn);
  g_byte_array_unref (atv);
}

static void
frida_build_validity (GByteArray * out)
{
  const time_t lifetime_seconds = 15780000;
  GByteArray * inner;
  time_t not_before, not_after;

  inner = g_byte_array_new ();

  not_before = time (NULL);
  not_after = not_before + lifetime_seconds;

  frida_der_append_utctime (inner, not_before);
  frida_der_append_utctime (inner, not_after);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_subject_public_key_info (GByteArray * out, const GByteArray * rsa_public_key)
{
  GByteArray * inner = g_byte_array_new ();

  frida_build_algorithm_identifier (inner, frida_oid_rsa_encryption, sizeof (frida_oid_rsa_encryption));
  frida_der_append_bit_string (inner, rsa_public_key->data, rsa_public_key->len);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_signed_certificate (GByteArray * out, const GByteArray * tbs, const GByteArray * signature)
{
  GByteArray * inner = g_byte_array_new ();

  g_byte_array_append (inner, tbs->data, tbs->len);
  frida_build_algorithm_identifier (inner, frida_oid_sha256_with_rsa, sizeof (frida_oid_sha256_with_rsa));
  frida_der_append_bit_string (inner, signature->data, signature->len);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_algorithm_identifier (GByteArray * out, const guint8 * oid, gsize oid_length)
{
  GByteArray * inner = g_byte_array_new ();

  frida_der_append_oid (inner, oid, oid_length);
  frida_der_append_null (inner);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static void
frida_build_pkcs8_private_key_info (GByteArray * out, const GByteArray * rsa_private_key)
{
  GByteArray * inner = g_byte_array_new ();

  frida_der_append_integer_uint (inner, 0);
  frida_build_algorithm_identifier (inner, frida_oid_rsa_encryption, sizeof (frida_oid_rsa_encryption));
  frida_der_append_tlv (inner, 0x04, rsa_private_key->data, rsa_private_key->len);

  frida_der_append_tlv (out, 0x30, inner->data, inner->len);

  g_byte_array_unref (inner);
}

static gchar *
frida_pem_encode (const gchar * type, const guint8 * der, gsize der_length)
{
  GString * pem;
  gchar * base64;
  gsize length, i;

  base64 = g_base64_encode (der, der_length);
  length = strlen (base64);

  pem = g_string_sized_new (length + 128);

  g_string_append_printf (pem, "-----BEGIN %s-----\n", type);

  for (i = 0; i < length; i += 64)
  {
    gsize chunk = MIN (64, length - i);
    g_string_append_len (pem, base64 + i, chunk);
    g_string_append_c (pem, '\n');
  }

  g_string_append_printf (pem, "-----END %s-----\n", type);

  g_free (base64);

  return g_string_free (pem, FALSE);
}

static void
frida_der_append_tlv (GByteArray * out, guint8 tag, const guint8 * value, gsize length)
{
  g_byte_array_append (out, &tag, 1);
  frida_der_append_length (out, length);
  if (length > 0)
    g_byte_array_append (out, value, length);
}

static void
frida_der_append_length (GByteArray * out, gsize length)
{
  gint i;
  guint8 buf[9], header;

  if (length < 0x80)
  {
    guint8 b = (guint8) length;
    g_byte_array_append (out, &b, 1);
    return;
  }

  i = 0;
  while (length > 0)
  {
    buf[++i] = (guint8) (length & 0xff);
    length >>= 8;
  }
  header = (guint8) (0x80 | i);
  g_byte_array_append (out, &header, 1);
  while (i > 0)
  {
    g_byte_array_append (out, &buf[i], 1);
    i--;
  }
}

static void
frida_der_append_integer_uint (GByteArray * out, guint64 value)
{
  gint i;
  guint8 buf[9];
  gint start;

  for (i = 7; i >= 0; i--)
  {
    buf[i + 1] = (guint8) (value & 0xff);
    value >>= 8;
  }
  buf[0] = 0;

  start = 0;
  while (start < 8 && buf[start] == 0 && (buf[start + 1] & 0x80) == 0)
    start++;

  frida_der_append_tlv (out, 0x02, buf + start, (gsize) (9 - start));
}

static void
frida_der_append_null (GByteArray * out)
{
  frida_der_append_tlv (out, 0x05, NULL, 0);
}

static void
frida_der_append_oid (GByteArray * out, const guint8 * oid, gsize length)
{
  frida_der_append_tlv (out, 0x06, oid, length);
}

static void
frida_der_append_utctime (GByteArray * out, time_t t)
{
  struct tm tm;
  gchar buf[16];

  gmtime_r (&t, &tm);
  g_snprintf (buf, sizeof (buf), "%02d%02d%02d%02d%02d%02dZ",
      tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday,
      tm.tm_hour, tm.tm_min, tm.tm_sec);

  frida_der_append_tlv (out, 0x17, (const guint8 *) buf, strlen (buf));
}

static void
frida_der_append_bit_string (GByteArray * out, const guint8 * bytes, gsize length)
{
  guint8 tag = 0x03;
  guint8 unused_bits = 0;

  g_byte_array_append (out, &tag, 1);
  frida_der_append_length (out, length + 1);
  g_byte_array_append (out, &unused_bits, 1);
  if (length > 0)
    g_byte_array_append (out, bytes, length);
}

#else

void
_frida_generate_certificate (guint8 ** cert_der, gint * cert_der_length, gchar ** cert_pem, gchar ** key_pem)
{
  X509 * x509;
  X509_NAME * name;
  EVP_PKEY * pkey;
  BIGNUM * e;
  RSA * rsa;
  BIO * bio;
  guint8 * der;
  long n;

  x509 = X509_new ();

  ASN1_INTEGER_set (X509_get_serialNumber (x509), 1);
  X509_gmtime_adj (X509_get_notBefore (x509), 0);
  X509_gmtime_adj (X509_get_notAfter (x509), 15780000);

  name = X509_get_subject_name (x509);
  X509_NAME_add_entry_by_txt (name, "C", MBSTRING_ASC, (const unsigned char *) "CA", -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "O", MBSTRING_ASC, (const unsigned char *) "Frida", -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "CN", MBSTRING_ASC, (const unsigned char *) "lolcathost", -1, -1, 0);
  X509_set_issuer_name (x509, name);

  pkey = EVP_PKEY_new ();
  e = BN_new ();
  BN_set_word (e, RSA_F4);
  rsa = RSA_new ();
  RSA_generate_key_ex (rsa, 2048, e, NULL);
  EVP_PKEY_set1_RSA (pkey, g_steal_pointer (&rsa));
  BN_free (e);
  X509_set_pubkey (x509, pkey);

  X509_sign (x509, pkey, EVP_sha256 ());

  bio = BIO_new (BIO_s_mem ());
  i2d_X509_bio (bio, x509);
  n = BIO_get_mem_data (bio, (guint8 **) &der);
  *cert_der = g_memdup2 (der, n);
  *cert_der_length = n;
  BIO_free (g_steal_pointer (&bio));

  bio = BIO_new (BIO_s_mem ());
  PEM_write_bio_X509 (bio, x509);
  *cert_pem = frida_steal_bio_to_string (&bio);

  bio = BIO_new (BIO_s_mem ());
  PEM_write_bio_PrivateKey (bio, pkey, NULL, NULL, 0, NULL, NULL);
  *key_pem = frida_steal_bio_to_string (&bio);

  EVP_PKEY_free (pkey);
  X509_free (x509);
}

static gchar *
frida_steal_bio_to_string (BIO ** bio)
{
  gchar * result;
  long n;
  char * str;

  n = BIO_get_mem_data (*bio, &str);
  result = g_strndup (str, n);

  BIO_free (g_steal_pointer (bio));

  return result;
}

#endif

void
_frida_sctp_connection_initialize_sctp_backend (void)
{
  const int msec_per_sec = 1000;

  usrsctp_init_nothreads (0, frida_on_connection_output, frida_on_debug_printf);

  usrsctp_sysctl_set_sctp_sendspace (256 * 1024);
  usrsctp_sysctl_set_sctp_recvspace (256 * 1024);

  usrsctp_sysctl_set_sctp_ecn_enable (FALSE);
  usrsctp_sysctl_set_sctp_pr_enable (TRUE);
  usrsctp_sysctl_set_sctp_auth_enable (FALSE);
  usrsctp_sysctl_set_sctp_asconf_enable (FALSE);

  usrsctp_sysctl_set_sctp_max_burst_default (10);

  usrsctp_sysctl_set_sctp_max_chunks_on_queue (10 * 1024);

  usrsctp_sysctl_set_sctp_delayed_sack_time_default (20);

  usrsctp_sysctl_set_sctp_heartbeat_interval_default (10 * msec_per_sec);

  usrsctp_sysctl_set_sctp_rto_max_default (10 * msec_per_sec);
  usrsctp_sysctl_set_sctp_rto_min_default (1 * msec_per_sec);
  usrsctp_sysctl_set_sctp_rto_initial_default (1 * msec_per_sec);
  usrsctp_sysctl_set_sctp_init_rto_max_default (10 * msec_per_sec);

  usrsctp_sysctl_set_sctp_init_rtx_max_default (5);
  usrsctp_sysctl_set_sctp_assoc_rtx_max_default (5);
  usrsctp_sysctl_set_sctp_path_rtx_max_default (5);

  usrsctp_sysctl_set_sctp_nr_outgoing_streams_default (1024);

  usrsctp_sysctl_set_sctp_initial_cwnd (10);
}

static int
frida_on_connection_output (void * addr, void * buffer, size_t length, uint8_t tos, uint8_t set_df)
{
  FridaSctpConnection * connection = addr;

  _frida_sctp_connection_emit_transport_packet (connection, buffer, (gint) length);

  return 0;
}

static void
frida_on_debug_printf (const char * format, ...)
{
  g_printerr ("[SCTP] %s\n", format);
}

void *
_frida_sctp_connection_create_sctp_socket (FridaSctpConnection * self)
{
  struct socket * sock;
  struct linger linger;
  int nodelay;
  struct sctp_event ev;
  const uint16_t event_types[] = {
    SCTP_ASSOC_CHANGE,
    SCTP_PEER_ADDR_CHANGE,
    SCTP_REMOTE_ERROR,
    SCTP_SHUTDOWN_EVENT,
    SCTP_ADAPTATION_INDICATION,
    SCTP_STREAM_RESET_EVENT,
    SCTP_SENDER_DRY_EVENT,
    SCTP_STREAM_CHANGE_EVENT,
    SCTP_SEND_FAILED_EVENT,
  };
  guint i;
  int recv_rcvinfo;
  struct sctp_assoc_value assoc;

  usrsctp_register_address (self);

  sock = usrsctp_socket (AF_CONN, SOCK_STREAM, IPPROTO_SCTP, NULL, NULL, 0, NULL);
  usrsctp_set_upcall (sock, frida_on_upcall, self);
  usrsctp_set_non_blocking (sock, TRUE);

  linger.l_onoff = TRUE;
  linger.l_linger = 0;
  usrsctp_setsockopt (sock, SOL_SOCKET, SO_LINGER, &linger, sizeof (linger));

  nodelay = TRUE;
  usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof (nodelay));

  ev.se_assoc_id = SCTP_ALL_ASSOC;
  ev.se_on = TRUE;
  for (i = 0; i != G_N_ELEMENTS (event_types); i++)
  {
    ev.se_type = event_types[i];
    usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_EVENT, &ev, sizeof (ev));
  }

  recv_rcvinfo = TRUE;
  usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_RECVRCVINFO, &recv_rcvinfo, sizeof (recv_rcvinfo));

  assoc.assoc_id = SCTP_ALL_ASSOC;
  assoc.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
  usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &assoc, sizeof (assoc));

  return sock;
}

void
_frida_sctp_connection_connect_sctp_socket (FridaSctpConnection * self, void * sock, guint16 port)
{
  struct sockaddr_conn addr;

#ifdef HAVE_SCONN_LEN
  addr.sconn_len = sizeof (addr);
#endif
  addr.sconn_family = AF_CONN;
  addr.sconn_port = htons (port);
  addr.sconn_addr = self;

  usrsctp_bind (sock, (struct sockaddr *) &addr, sizeof (addr));

  usrsctp_connect (sock, (struct sockaddr *) &addr, sizeof (addr));
}

static void
frida_on_upcall (struct socket * sock, void * user_data, int flags)
{
  FridaSctpConnection * connection = user_data;

  _frida_sctp_connection_on_sctp_socket_events_changed (connection);
}

void
_frida_sctp_connection_close (void * sock)
{
  usrsctp_close (sock);
}

void
_frida_sctp_connection_shutdown (void * sock, FridaSctpShutdownType type, GError ** error)
{
  if (usrsctp_shutdown (sock, type) == -1)
  {
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "%s", g_strerror (errno));
  }
}

GIOCondition
_frida_sctp_connection_query_sctp_socket_events (void * sock)
{
  GIOCondition condition = 0;
  int events;

  events = usrsctp_get_events (sock);

  if ((events & SCTP_EVENT_READ) != 0)
    condition |= G_IO_IN;

  if ((events & SCTP_EVENT_WRITE) != 0)
    condition |= G_IO_OUT;

  if ((events & SCTP_EVENT_ERROR) != 0)
    condition |= G_IO_ERR;

  return condition;
}

void
_frida_sctp_connection_handle_transport_packet (FridaSctpConnection * self, guint8 * data, gint data_length)
{
  usrsctp_conninput (self, data, data_length, 0);
}

gssize
_frida_sctp_connection_recv (void * sock, guint8 * buffer, gint buffer_length, guint16 * stream_id, FridaPayloadProtocolId * protocol_id,
    FridaSctpMessageFlags * message_flags, GError ** error)
{
  gssize n;
  struct sockaddr_conn from;
  socklen_t from_length;
  struct sctp_rcvinfo info;
  socklen_t info_length;
  unsigned int info_type;
  int msg_flags;

  from_length = sizeof (from);
  info_length = sizeof (info);
  info_type = SCTP_RECVV_NOINFO;
  msg_flags = 0;

  n = usrsctp_recvv (sock, buffer, buffer_length, (struct sockaddr *) &from, &from_length, &info, &info_length, &info_type, &msg_flags);
  if (n == -1)
    goto propagate_usrsctp_error;

  if (info_type == SCTP_RECVV_RCVINFO)
  {
    *stream_id = info.rcv_sid;
    *protocol_id = ntohl (info.rcv_ppid);
  }
  else
  {
    *stream_id = 0;
    *protocol_id = FRIDA_PAYLOAD_PROTOCOL_ID_NONE;
  }

  *message_flags = 0;

  if ((msg_flags & MSG_EOR) != 0)
    *message_flags |= FRIDA_SCTP_MESSAGE_FLAGS_END_OF_RECORD;

  if ((msg_flags & MSG_NOTIFICATION) != 0)
    *message_flags |= FRIDA_SCTP_MESSAGE_FLAGS_NOTIFICATION;

  return n;

propagate_usrsctp_error:
  {
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "%s", g_strerror (errno));
    return -1;
  }
}

gssize
_frida_sctp_connection_send (void * sock, guint16 stream_id, FridaPayloadProtocolId protocol_id, guint8 * data, gint data_length,
      GError ** error)
{
  gssize n;
  struct sctp_sendv_spa spa;
  struct sctp_sndinfo * si;

  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  si = &spa.sendv_sndinfo;
  si->snd_sid = stream_id;
  si->snd_flags = SCTP_EOR;
  si->snd_ppid = htonl (protocol_id);
  si->snd_context = 0;
  si->snd_assoc_id = 0;

  n = usrsctp_sendv (sock, data, data_length, NULL, 0, &spa, sizeof (spa), SCTP_SENDV_SPA, 0);
  if (n == -1)
    goto propagate_usrsctp_error;

  return n;

propagate_usrsctp_error:
  {
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "%s", g_strerror (errno));
    return -1;
  }
}

gint
_frida_sctp_timer_source_get_timeout (void)
{
  return usrsctp_get_timeout ();
}

void
_frida_sctp_timer_source_process_timers (guint32 elapsed_msec)
{
  usrsctp_handle_timers (elapsed_msec);
}

#endif /* HAVE_NICE */
