/**
 * marshalling_tools.c:
 *
 * C library containing data marshalling utilities supporting Kmyth applications 
 * using TPM 2.0
 */

#include "tpm/marshalling_tools.h"

#include <string.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <tss2/tss2_mu.h>

#include "defines.h"

//############################################################################
// parse_ski_bytes
//############################################################################
int parse_ski_bytes(uint8_t * input, size_t input_length, Ski * output)
{

  if (input == NULL)
  {
    kmyth_log(LOG_ERR, "NULL input cannot be parsed ... exiting");
    return 1;
  }

  uint8_t *position = input;
  size_t remaining = input_length;
  Ski temp_ski = get_default_ski();

  // read in (parse out) 'raw' (encoded) PCR selection list block
  uint8_t *raw_pcr_select_list_data = NULL;
  size_t raw_pcr_select_list_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_pcr_select_list_data,
                      &raw_pcr_select_list_size,
                      KMYTH_DELIM_PCR_SELECTION_LIST,
                      strlen(KMYTH_DELIM_PCR_SELECTION_LIST),
                      KMYTH_DELIM_STORAGE_KEY_PUBLIC,
                      strlen(KMYTH_DELIM_STORAGE_KEY_PUBLIC)))
  {
    kmyth_log(LOG_ERR, "get PCR selection list error ... exiting");
    free(raw_pcr_select_list_data);
    return 1;
  }

  // read in (parse out) 'raw' (encoded) public data block for the storage key
  uint8_t *raw_sk_pub_data = NULL;
  size_t raw_sk_pub_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_sk_pub_data,
                      &raw_sk_pub_size,
                      KMYTH_DELIM_STORAGE_KEY_PUBLIC,
                      strlen(KMYTH_DELIM_STORAGE_KEY_PUBLIC),
                      KMYTH_DELIM_STORAGE_KEY_PRIVATE,
                      strlen(KMYTH_DELIM_STORAGE_KEY_PRIVATE)))
  {
    kmyth_log(LOG_ERR, "get storage key public error ... exiting");
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    return 1;
  }

  // read in (parse out) 'raw' (encoded) private data block for the storage key
  uint8_t *raw_sk_priv_data = NULL;
  size_t raw_sk_priv_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_sk_priv_data,
                      &raw_sk_priv_size,
                      KMYTH_DELIM_STORAGE_KEY_PRIVATE,
                      strlen(KMYTH_DELIM_STORAGE_KEY_PRIVATE),
                      KMYTH_DELIM_CIPHER_SUITE,
                      strlen(KMYTH_DELIM_CIPHER_SUITE)))
  {
    kmyth_log(LOG_ERR, "get storage key private error ... exiting");
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    return 1;
  }

  // read in (parse out) cipher suite string data block for the storage key
  uint8_t *raw_cipher_str_data = NULL;
  size_t raw_cipher_str_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_cipher_str_data,
                      &raw_cipher_str_size,
                      KMYTH_DELIM_CIPHER_SUITE,
                      strlen(KMYTH_DELIM_CIPHER_SUITE),
                      KMYTH_DELIM_SYM_KEY_PUBLIC,
                      strlen(KMYTH_DELIM_SYM_KEY_PUBLIC)))
  {
    kmyth_log(LOG_ERR, "get cipher string error ... exiting");
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_cipher_str_data);
    return 1;
  }

  // create cipher suite struct
  raw_cipher_str_data[raw_cipher_str_size - 1] = '\0';
  temp_ski.cipher =
    kmyth_get_cipher_t_from_string((char *) raw_cipher_str_data);
  if (temp_ski.cipher.cipher_name == NULL)
  {
    kmyth_log(LOG_ERR, "cipher_t init error ... exiting");
    free_ski(&temp_ski);
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_cipher_str_data);
    return 1;
  }
  free(raw_cipher_str_data);
  raw_cipher_str_data = NULL;
  // read in (parse out) 'raw' (encoded) public data block for the wrapping key
  uint8_t *raw_sym_pub_data = NULL;
  size_t raw_sym_pub_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_sym_pub_data,
                      &raw_sym_pub_size,
                      KMYTH_DELIM_SYM_KEY_PUBLIC,
                      strlen(KMYTH_DELIM_SYM_KEY_PUBLIC),
                      KMYTH_DELIM_SYM_KEY_PRIVATE,
                      strlen(KMYTH_DELIM_SYM_KEY_PRIVATE)))
  {
    kmyth_log(LOG_ERR, "get symmetric key public error ... exiting");
    free_ski(&temp_ski);
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_sym_pub_data);
    return 1;
  }

  // read in (parse out) raw (encoded) private data block for the wrapping key
  unsigned char *raw_sym_priv_data = NULL;
  size_t raw_sym_priv_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_sym_priv_data,
                      &raw_sym_priv_size,
                      KMYTH_DELIM_SYM_KEY_PRIVATE,
                      strlen(KMYTH_DELIM_SYM_KEY_PRIVATE),
                      KMYTH_DELIM_ENC_DATA, strlen(KMYTH_DELIM_ENC_DATA)))
  {
    kmyth_log(LOG_ERR, "get symmetric key private error ... exiting");
    free_ski(&temp_ski);
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_sym_pub_data);
    free(raw_sym_priv_data);
    return 1;
  }

  // read in (parse out) raw (encoded) encrypted data block
  unsigned char *raw_enc_data = NULL;
  size_t raw_enc_size = 0;

  if (get_block_bytes((char **) &position,
                      &remaining,
                      &raw_enc_data, &raw_enc_size,
                      KMYTH_DELIM_ENC_DATA,
                      strlen(KMYTH_DELIM_ENC_DATA),
                      KMYTH_DELIM_END_FILE, strlen(KMYTH_DELIM_END_FILE)))
  {
    kmyth_log(LOG_ERR, "getting encrypted data error ... exiting");
    free_ski(&temp_ski);
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_sym_pub_data);
    free(raw_sym_priv_data);
    free(raw_enc_data);
    return 1;
  }

  if (strncmp
      ((char *) position, KMYTH_DELIM_END_FILE, strlen(KMYTH_DELIM_END_FILE))
      || remaining != strlen(KMYTH_DELIM_END_FILE))
  {
    kmyth_log(LOG_ERR, "unable to find the end delimiter ... exiting");
    free_ski(&temp_ski);
    free(raw_pcr_select_list_data);
    free(raw_sk_pub_data);
    free(raw_sk_priv_data);
    free(raw_sym_pub_data);
    free(raw_sym_priv_data);
    free(raw_enc_data);
    return 1;
  }

  //We are done with position. It was marking our place in input, which is freed by the caller
  position = NULL;

  int retval = 0;

  // decode PCR selection list struct
  uint8_t *decoded_pcr_select_list_data = NULL;
  size_t decoded_pcr_select_list_size = 0;
  size_t decoded_pcr_select_list_offset = 0;

  retval |= decodeBase64Data(raw_pcr_select_list_data,
                             raw_pcr_select_list_size,
                             &decoded_pcr_select_list_data,
                             &decoded_pcr_select_list_size);
  free(raw_pcr_select_list_data);
  raw_pcr_select_list_data = NULL;

  // decode public data block for storage key
  uint8_t *decoded_sk_pub_data = NULL;
  size_t decoded_sk_pub_size = 0;
  size_t decoded_sk_pub_offset = 0;

  retval |= decodeBase64Data(raw_sk_pub_data,
                             raw_sk_pub_size,
                             &decoded_sk_pub_data, &decoded_sk_pub_size);
  free(raw_sk_pub_data);
  raw_sk_pub_data = NULL;

  // decode encrypted private data block for storage key
  uint8_t *decoded_sk_priv_data = NULL;
  size_t decoded_sk_priv_size = 0;
  size_t decoded_sk_priv_offset = 0;

  retval |= decodeBase64Data(raw_sk_priv_data,
                             raw_sk_priv_size,
                             &decoded_sk_priv_data, &decoded_sk_priv_size);
  free(raw_sk_priv_data);
  raw_sk_priv_data = NULL;

  // decode public data block for symmetric wrapping key
  uint8_t *decoded_sym_pub_data = NULL;
  size_t decoded_sym_pub_size = 0;
  size_t decoded_sym_pub_offset = 0;

  retval |= decodeBase64Data(raw_sym_pub_data,
                             raw_sym_pub_size,
                             &decoded_sym_pub_data, &decoded_sym_pub_size);
  free(raw_sym_pub_data);
  raw_sym_pub_data = NULL;

  // decode encrypted private data block for symmetric wrapping key
  uint8_t *decoded_sym_priv_data = NULL;
  size_t decoded_sym_priv_size = 0;
  size_t decoded_sym_priv_offset = 0;

  retval |= decodeBase64Data(raw_sym_priv_data,
                             raw_sym_priv_size,
                             &decoded_sym_priv_data, &decoded_sym_priv_size);
  free(raw_sym_priv_data);
  raw_sym_priv_data = NULL;

  // decode the encrypted data block
  retval |= decodeBase64Data(raw_enc_data,
                             raw_enc_size, &temp_ski.enc_data,
                             &temp_ski.enc_data_size);
  free(raw_enc_data);
  raw_enc_data = NULL;

  if (retval)
  {
    kmyth_log(LOG_ERR, "base64 decode error ... exiting");
  }
  else
  {
    retval = unmarshal_skiObjects(&temp_ski.pcr_list,
                                  decoded_pcr_select_list_data,
                                  decoded_pcr_select_list_size,
                                  decoded_pcr_select_list_offset,
                                  &temp_ski.sk_pub,
                                  decoded_sk_pub_data,
                                  decoded_sk_pub_size,
                                  decoded_sk_pub_offset,
                                  &temp_ski.sk_priv,
                                  decoded_sk_priv_data,
                                  decoded_sk_priv_size,
                                  decoded_sk_priv_offset,
                                  &temp_ski.wk_pub,
                                  decoded_sym_pub_data,
                                  decoded_sym_pub_size,
                                  decoded_sym_pub_offset,
                                  &temp_ski.wk_priv,
                                  decoded_sym_priv_data,
                                  decoded_sym_priv_size,
                                  decoded_sym_priv_offset);
    if (retval)
    {
      kmyth_log(LOG_ERR, "unmarshal .ski object error ... exiting");
    }
  }

  free(decoded_pcr_select_list_data);
  free(decoded_sk_pub_data);
  free(decoded_sk_priv_data);
  free(decoded_sym_pub_data);
  free(decoded_sym_priv_data);
  *output = temp_ski;
  return retval;
}

//############################################################################
// create_ski_bytes
//############################################################################
int create_ski_bytes(Ski input, uint8_t ** output, size_t * output_length)
{
  // marshal data contained in TPM sized buffers (TPM2B_PUBLIC / TPM2B_PRIVATE)
  // and structs (TPML_PCR_SELECTION)
  // Note: must account for two extra bytes to include the buffer's size value
  //       in the TPM2B_* sized buffer cases
  size_t pcr_select_size = sizeof(input.pcr_list);
  size_t pcr_select_offset = 0;
  uint8_t *pcr_select_data =
    (uint8_t *) calloc(pcr_select_size, sizeof(uint8_t));

  if (pcr_select_data == NULL)
  {
    kmyth_log(LOG_ERR,
              "unable to allocate memory for PCR select data ... exiting");
    return 1;
  }

  size_t sk_pub_size = input.sk_pub.size + 2;
  size_t sk_pub_offset = 0;
  uint8_t *sk_pub_data = (uint8_t *) malloc(sk_pub_size);

  if (sk_pub_data == NULL)
  {
    kmyth_log(LOG_ERR,
              "unable to allocate memory for storage key public data ... exiting");
    free(pcr_select_data);
    return 1;
  }

  size_t sk_priv_size = input.sk_priv.size + 2;
  size_t sk_priv_offset = 0;
  uint8_t *sk_priv_data = (uint8_t *) malloc(sk_priv_size);

  if (sk_priv_data == NULL)
  {
    kmyth_log(LOG_ERR,
              "unable to allocate memory for storage key private data ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    return 1;
  }

  size_t wk_pub_size = input.wk_pub.size + 2;
  size_t wk_pub_offset = 0;
  uint8_t *wk_pub_data = (uint8_t *) malloc(wk_pub_size);

  if (wk_pub_data == NULL)
  {
    kmyth_log(LOG_ERR,
              "unable to allocate memory for wrapping key public data ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    free(sk_priv_data);
    return 1;
  }

  size_t wk_priv_size = input.wk_priv.size + 2;
  size_t wk_priv_offset = 0;
  uint8_t *wk_priv_data = (uint8_t *) malloc(wk_priv_size);

  if (wk_priv_data == NULL)
  {
    kmyth_log(LOG_ERR,
              "unable to allocate memory for wrapping key private data ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    free(sk_priv_data);
    free(wk_pub_data);
    return 1;
  }

  if (marshal_skiObjects(&input.pcr_list,
                         &pcr_select_data,
                         &pcr_select_size,
                         pcr_select_offset,
                         &input.sk_pub,
                         &sk_pub_data,
                         &sk_pub_size,
                         sk_pub_offset,
                         &input.sk_priv,
                         &sk_priv_data,
                         &sk_priv_size,
                         sk_priv_offset,
                         &input.wk_pub,
                         &wk_pub_data,
                         &wk_pub_size,
                         wk_pub_offset,
                         &input.wk_priv,
                         &wk_priv_data, &wk_priv_size, wk_priv_offset))
  {
    kmyth_log(LOG_ERR, "unable to marshal data for ski file ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    free(sk_priv_data);
    free(wk_pub_data);
    free(wk_priv_data);
    return 1;
  }

  // validate that all data to be written is non-NULL and non-empty
  if (pcr_select_data == NULL ||
      pcr_select_size == 0 ||
      sk_pub_data == NULL ||
      sk_pub_size == 0 ||
      sk_priv_data == NULL ||
      sk_priv_size == 0 ||
      wk_pub_data == NULL ||
      wk_pub_size == 0 ||
      wk_priv_data == NULL ||
      wk_priv_size == 0 ||
      input.cipher.cipher_name == NULL ||
      strlen(input.cipher.cipher_name) == 0 ||
      input.enc_data == NULL || input.enc_data_size == 0)
  {
    kmyth_log(LOG_ERR, "cannot write empty sections ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    free(sk_priv_data);
    free(wk_pub_data);
    free(wk_priv_data);
    return 1;
  }

  //Encode each portion of the file in base64
  uint8_t *pcr64_select_data = NULL;
  size_t pcr64_select_size = 0;
  uint8_t *sk64_pub_data = NULL;
  size_t sk64_pub_size = 0;
  uint8_t *sk64_priv_data = NULL;
  size_t sk64_priv_size = 0;
  uint8_t *wk64_pub_data = NULL;
  size_t wk64_pub_size = 0;
  uint8_t *wk64_priv_data = NULL;
  size_t wk64_priv_size = 0;
  uint8_t *enc64_data = NULL;
  size_t enc64_data_size = 0;

  if (encodeBase64Data
      (pcr_select_data, pcr_select_size, &pcr64_select_data, &pcr64_select_size)
      || encodeBase64Data(sk_pub_data, sk_pub_size, &sk64_pub_data,
                          &sk64_pub_size)
      || encodeBase64Data(sk_priv_data, sk_priv_size, &sk64_priv_data,
                          &sk64_priv_size)
      || encodeBase64Data(wk_pub_data, wk_pub_size, &wk64_pub_data,
                          &wk64_pub_size)
      || encodeBase64Data(wk_priv_data, wk_priv_size, &wk64_priv_data,
                          &wk64_priv_size)
      || encodeBase64Data(input.enc_data, input.enc_data_size, &enc64_data,
                          &enc64_data_size))
  {
    kmyth_log(LOG_ERR, "error base64 encoding ski string ... exiting");
    free(pcr_select_data);
    free(sk_pub_data);
    free(sk_priv_data);
    free(wk_pub_data);
    free(wk_priv_data);
    free(pcr64_select_data);
    free(sk64_pub_data);
    free(sk64_priv_data);
    free(wk64_pub_data);
    free(wk64_priv_data);
    free(enc64_data);
    return 1;
  }

  free(pcr_select_data);
  pcr_select_data = NULL;
  free(sk_pub_data);
  sk_pub_data = NULL;
  free(sk_priv_data);
  sk_priv_data = NULL;
  free(wk_pub_data);
  wk_pub_data = NULL;
  free(wk_priv_data);
  wk_priv_data = NULL;

  //At this point the data is all formatted, it's time to create the string

  uint8_t *out = NULL;
  size_t out_length = 0;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_PCR_SELECTION_LIST,
         strlen(KMYTH_DELIM_PCR_SELECTION_LIST));
  concat(&out, &out_length, pcr64_select_data, pcr64_select_size);
  free(pcr64_select_data);
  pcr64_select_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_STORAGE_KEY_PUBLIC,
         strlen(KMYTH_DELIM_STORAGE_KEY_PUBLIC));
  concat(&out, &out_length, sk64_pub_data, sk64_pub_size);
  free(sk64_pub_data);
  sk64_pub_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_STORAGE_KEY_PRIVATE,
         strlen(KMYTH_DELIM_STORAGE_KEY_PRIVATE));
  concat(&out, &out_length, sk64_priv_data, sk64_priv_size);
  free(sk64_priv_data);
  sk64_priv_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_CIPHER_SUITE,
         strlen(KMYTH_DELIM_CIPHER_SUITE));
  concat(&out, &out_length, (uint8_t *) input.cipher.cipher_name,
         strlen(input.cipher.cipher_name));
  concat(&out, &out_length, (uint8_t *) "\n", 1);

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_SYM_KEY_PUBLIC,
         strlen(KMYTH_DELIM_SYM_KEY_PUBLIC));
  concat(&out, &out_length, wk64_pub_data, wk64_pub_size);
  free(wk64_pub_data);
  wk64_pub_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_SYM_KEY_PRIVATE,
         strlen(KMYTH_DELIM_SYM_KEY_PRIVATE));
  concat(&out, &out_length, wk64_priv_data, wk64_priv_size);
  free(wk64_priv_data);
  wk64_priv_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_ENC_DATA,
         strlen(KMYTH_DELIM_ENC_DATA));
  concat(&out, &out_length, enc64_data, enc64_data_size);
  free(enc64_data);
  enc64_data = NULL;

  concat(&out, &out_length, (uint8_t *) KMYTH_DELIM_END_FILE,
         strlen(KMYTH_DELIM_END_FILE));

  *output = out;
  *output_length = out_length;

  return 0;
}

void free_ski(Ski * ski)
{
  free(ski->enc_data);
  ski->enc_data = NULL;
  ski->enc_data_size = 0;
}

Ski get_default_ski(void)
{
  Ski ret = {
    .pcr_list = {.count = 0,},
    .sk_pub = {.size = 0,},
    .sk_priv = {.size = 0,},
    .cipher = {.cipher_name = NULL,},
    .wk_pub = {.size = 0},
    .wk_priv = {.size = 0},
    .enc_data = NULL,
    .enc_data_size = 0
  };
  return (ret);

}

//############################################################################
// marshal_skiObjects()
//############################################################################
int marshal_skiObjects(TPML_PCR_SELECTION * pcr_selection_struct,
                       uint8_t ** pcr_selection_struct_data,
                       size_t * pcr_selection_struct_data_size,
                       size_t pcr_selection_struct_data_offset,
                       TPM2B_PUBLIC * storage_key_public_blob,
                       uint8_t ** storage_key_public_data,
                       size_t * storage_key_public_data_size,
                       size_t storage_key_public_data_offset,
                       TPM2B_PRIVATE * storage_key_private_blob,
                       uint8_t ** storage_key_private_data,
                       size_t * storage_key_private_data_size,
                       size_t storage_key_private_data_offset,
                       TPM2B_PUBLIC * sealed_key_public_blob,
                       uint8_t ** sealed_key_public_data,
                       size_t * sealed_key_public_data_size,
                       size_t sealed_key_public_data_offset,
                       TPM2B_PRIVATE * sealed_key_private_blob,
                       uint8_t ** sealed_key_private_data,
                       size_t * sealed_key_private_data_size,
                       size_t sealed_key_private_data_offset)
{
  // Validate that all input data structures to be packed in preparation
  // for writing to a .ski file are both non-NULL and non-empty.
  if (pcr_selection_struct == NULL ||
      storage_key_public_blob == NULL ||
      storage_key_private_blob == NULL ||
      sealed_key_public_blob == NULL ||
      sealed_key_private_blob == NULL ||
      storage_key_public_blob->size == 0 ||
      storage_key_private_blob->size == 0 ||
      sealed_key_public_blob->size == 0 || sealed_key_private_blob->size == 0)
  {
    kmyth_log(LOG_ERR, "input structs to be packed NULL or empty ... exiting");
    return 1;
  }

  // Marshal (pack) TPM PCR selection list struct
  if (*pcr_selection_struct_data == NULL)
  {
    kmyth_log(LOG_ERR, "unallocated PCR select list data ... exiting");
    return 1;
  }
  if (pack_pcr(pcr_selection_struct,
               *pcr_selection_struct_data,
               *pcr_selection_struct_data_size,
               pcr_selection_struct_data_offset))
  {
    kmyth_log(LOG_ERR, "error packing PCR select struct ... exiting");
    return 1;
  }

  // Marshal (pack) public data buffer for storage key (SK)
  if (*storage_key_public_data == NULL)
  {
    kmyth_log(LOG_ERR, "unallocated SK public byte array ... exiting");
    return 1;
  }
  if (pack_public(storage_key_public_blob,
                  *storage_key_public_data,
                  *storage_key_public_data_size,
                  storage_key_public_data_offset))
  {
    kmyth_log(LOG_ERR, "error packing SK public blob ... exiting");
    return 1;
  }

  // Marshal (pack) private data buffer for storage key (SK)
  if (*storage_key_private_data == NULL)
  {
    kmyth_log(LOG_ERR, "unallocated SK private byte array ... exiting");
    return 1;
  }
  if (pack_private(storage_key_private_blob,
                   *storage_key_private_data,
                   *storage_key_private_data_size,
                   storage_key_private_data_offset))
  {
    kmyth_log(LOG_ERR, "error packing SK private blob ... exiting");
    return 1;
  }

  // Marshal (pack) public data buffer for sealed wrapping key
  if (*sealed_key_public_data == NULL)
  {
    kmyth_log(LOG_ERR, "unallocated sealed key public byte array ... exiting");
    return 1;
  }
  if (pack_public(sealed_key_public_blob,
                  *sealed_key_public_data,
                  *sealed_key_public_data_size, sealed_key_public_data_offset))
  {
    kmyth_log(LOG_ERR, "error packing sealed key public blob ... exiting");
    return 1;
  }

  // Marshal (pack) private data buffer for sealed wrapping key
  if (*sealed_key_private_data == NULL)
  {
    kmyth_log(LOG_ERR, "unalloc'd sealed key private byte array ... exiting");
    return 1;
  }

  if (pack_private(sealed_key_private_blob,
                   *sealed_key_private_data,
                   *sealed_key_private_data_size,
                   sealed_key_private_data_offset))
  {
    kmyth_log(LOG_ERR, "error packing sealed key private blob ... exiting");
    return 1;
  }

  return 0;
}

//############################################################################
// unmarshal_skiObjects()
//############################################################################
int unmarshal_skiObjects(TPML_PCR_SELECTION * pcr_selection_struct,
                         uint8_t * pcr_selection_struct_data,
                         size_t pcr_selection_struct_data_size,
                         size_t pcr_selection_struct_data_offset,
                         TPM2B_PUBLIC * storage_key_public_blob,
                         uint8_t * storage_key_public_data,
                         size_t storage_key_public_data_size,
                         size_t storage_key_public_data_offset,
                         TPM2B_PRIVATE * storage_key_private_blob,
                         uint8_t * storage_key_private_data,
                         size_t storage_key_private_data_size,
                         size_t storage_key_private_data_offset,
                         TPM2B_PUBLIC * sealed_key_public_blob,
                         uint8_t * sealed_key_public_data,
                         size_t sealed_key_public_data_size,
                         size_t sealed_key_public_data_offset,
                         TPM2B_PRIVATE * sealed_key_private_blob,
                         uint8_t * sealed_key_private_data,
                         size_t sealed_key_private_data_size,
                         size_t sealed_key_private_data_offset)
{
  int retval = 0;

  // Unmarshal PCR selection list struct
  retval |= unpack_pcr(pcr_selection_struct,
                       pcr_selection_struct_data,
                       pcr_selection_struct_data_size,
                       pcr_selection_struct_data_offset);

  // Unmarshal public data for Kmyth storage key (SK)
  retval |= unpack_public(storage_key_public_blob,
                          storage_key_public_data,
                          storage_key_public_data_size,
                          storage_key_public_data_offset);

  // Unmarshal encrypted private data for Kmyth storage key (SK)
  retval |= unpack_private(storage_key_private_blob,
                           storage_key_private_data,
                           storage_key_private_data_size,
                           storage_key_private_data_offset);

  // Unmarshal public data for Kmyth sealed data object (sealed wrapping key)
  retval |= unpack_public(sealed_key_public_blob,
                          sealed_key_public_data,
                          sealed_key_public_data_size,
                          sealed_key_public_data_offset);

  // Unmarshal encrypted private data for Kmyth sealed data object
  retval |= unpack_private(sealed_key_private_blob,
                           sealed_key_private_data,
                           sealed_key_private_data_size,
                           sealed_key_private_data_offset);

  return retval;
}

//############################################################################
// pack_pcr()
//############################################################################
int pack_pcr(TPML_PCR_SELECTION * pcr_select_in,
             uint8_t * packed_data_out,
             size_t packed_data_out_size, size_t packed_data_out_offset)
{
  // "Marshal" input PCR selections into packed, platform independent format
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPML_PCR_SELECTION_Marshal(pcr_select_in,
                                               packed_data_out,
                                               packed_data_out_size,
                                               &packed_data_out_offset)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPML_PCR_SELECTION_Marshal(): 0x%08X ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// unpack_pcr()
//############################################################################
int unpack_pcr(TPML_PCR_SELECTION * pcr_select_out,
               uint8_t * packed_data_in,
               size_t packed_data_in_size, size_t packed_data_in_offset)
{
  // "Unmarshal" input packed (.ski) format into a TPML_PCR_SELECTION struct
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPML_PCR_SELECTION_Unmarshal(packed_data_in,
                                                 packed_data_in_size,
                                                 &packed_data_in_offset,
                                                 pcr_select_out)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPML_PCR_SELECTION_Unmarshal(): 0x%08x ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// pack_public()
//############################################################################
int pack_public(TPM2B_PUBLIC * public_blob_in,
                uint8_t * packed_data_out,
                size_t packed_data_out_size, size_t packed_data_out_offset)
{
  // "Marshal" input public blob into packed, platform independent format
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPM2B_PUBLIC_Marshal(public_blob_in,
                                         packed_data_out,
                                         packed_data_out_size,
                                         &packed_data_out_offset)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPM2B_PUBLIC_Marshal(): 0x%08X ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// unpack_public()
//############################################################################
int unpack_public(TPM2B_PUBLIC * public_blob_out,
                  uint8_t * packed_data_in,
                  size_t packed_data_in_size, size_t packed_data_in_offset)
{
  // "Unmarshal" input packed (.ski file) format into a TPM2B_PUBLIC struct
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPM2B_PUBLIC_Unmarshal(packed_data_in,
                                           packed_data_in_size,
                                           &packed_data_in_offset,
                                           public_blob_out)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPM2B_PUBLIC_Unmarshal(): 0x%08x ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// pack_private()
//############################################################################
int pack_private(TPM2B_PRIVATE * private_blob_in,
                 uint8_t * packed_data_out,
                 size_t packed_data_out_size, size_t packed_data_out_offset)
{
  // "Marshal" input private blob into packed, platform independent format
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPM2B_PRIVATE_Marshal(private_blob_in,
                                          packed_data_out,
                                          packed_data_out_size,
                                          &packed_data_out_offset)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPM2B_PRIVATE_Marshal(): 0x%08X ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// unpack_private()
//############################################################################
int unpack_private(TPM2B_PRIVATE * private_blob_out,
                   uint8_t * packed_data_in,
                   size_t packed_data_in_size, size_t packed_data_in_offset)
{
  // "Unmarshal" input packed (.ski file) format into a TPM2B_PRIVATE struct
  TSS2_RC rc = 0;

  if ((rc = Tss2_MU_TPM2B_PRIVATE_Unmarshal(packed_data_in,
                                            packed_data_in_size,
                                            &packed_data_in_offset,
                                            private_blob_out)))
  {
    kmyth_log(LOG_ERR,
              "Tss2_MU_TPM2B_PRIVATE_Unmarshal(): 0x%08x ... exiting", rc);
    return 1;
  }

  return 0;
}

//############################################################################
// unpack_uint32_to_str()
//############################################################################
int unpack_uint32_to_str(uint32_t uint_value, char **str_repr)
{
  if (asprintf(str_repr, "%c%c%c%c",
               ((uint8_t *) & uint_value)[3],
               ((uint8_t *) & uint_value)[2],
               ((uint8_t *) & uint_value)[1],
               ((uint8_t *) & uint_value)[0]) < 0)
  {
    kmyth_log(LOG_ERR, "error unpacking uint32 to string ... exiting");
    return 1;
  }

  return 0;
}
