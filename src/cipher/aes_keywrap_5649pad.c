/**
 * @file  aes_keywrap_5649pad.c
 *
 * @brief Implements AES Key Wrap with Padding (RFC 5649) for kmyth.
 */

#include "cipher/aes_keywrap_5649pad.h"

#include <openssl/evp.h>

#include "defines.h"

//##########################################################################
// aes_keywrap_5649pad_encrypt()
//##########################################################################
int aes_keywrap_5649pad_encrypt(unsigned char *key,
                                size_t key_len,
                                unsigned char *inData,
                                size_t inData_len, unsigned char **outData,
                                size_t * outData_len)
{
  // validate non-NULL and non-empty encryption key specified
  if (key == NULL || key_len == 0)
  {
    return 1;
  }

  // verify non-NULL, non-empty input plaintext buffer of valid size
  if (inData == NULL || inData_len == 0)
  {
    return 1;
  }
  if (inData_len > AES_KEYWRAP_5649PAD_MAX_DATA_LEN)
  {
    return 1;
  }

  // setup output ciphertext data buffer (outData)
  //   1. determine how many 8-byte blocks are required to hold the data
  //   2. add 8 to account for the 4 byte IV and 4 byte counter
  //   3. allocate memory based on the size calculation
  *outData_len = ((inData_len + 7) & ~7) + 8;
  *outData = NULL;
  *outData = malloc(*outData_len);
  if (*outData == NULL)
  {
    return 1;
  }

  // initialize the cipher context to match cipher suite being used
  //   - OpenSSL requires the WRAP_ALLOW flag be explicitly set to use key
  //     wrap modes through EVP.
  EVP_CIPHER_CTX *ctx;

  if (!(ctx = EVP_CIPHER_CTX_new()))
  {
    free(*outData);
    return 1;
  }
  EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
  int init_result = 0;

  switch (key_len)
  {
  case 16:
    init_result =
      EVP_EncryptInit_ex(ctx, EVP_aes_128_wrap_pad(), NULL, NULL, NULL);
    break;
  case 24:
    init_result =
      EVP_EncryptInit_ex(ctx, EVP_aes_192_wrap_pad(), NULL, NULL, NULL);
    break;
  case 32:
    init_result =
      EVP_EncryptInit_ex(ctx, EVP_aes_256_wrap_pad(), NULL, NULL, NULL);
    break;
  default:
    break;
  }
  if (!init_result)
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  // set the encryption key in the cipher context
  if (!EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  // track the ciphertext length separate from *outData_len because we
  // know in advance what *outData_len should be, but want to verify that
  // the output ciphertext length we actually end up with is as expected.
  //   - ciphertext_len: integer variable used to accumulate length result
  //   - tmp_len: integer variable used to get output size from EVP functions
  int ciphertext_len = 0;
  int tmp_len = 0;

  // encrypt (wrap) the input PT, put result in the output CT buffer
  if (!EVP_EncryptUpdate(ctx, *outData, &tmp_len, inData, inData_len))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }
  ciphertext_len = tmp_len;

  // OpenSSL requires a "finalize" operation
  if (!EVP_EncryptFinal_ex(ctx, (*outData) + ciphertext_len, &tmp_len))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }
  ciphertext_len += tmp_len;

  // verify that the resultant CT length matches expected (input PT length
  // plus 4-byte IV plus 4-byte counter + any necessary padding)
  if (ciphertext_len != *outData_len)
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  // now that the encryption is complete, clean-up cipher context
  EVP_CIPHER_CTX_free(ctx);

  return 0;
}

//##########################################################################
// aes_keywrap_5649pad_decrypt()
//##########################################################################
int aes_keywrap_5649pad_decrypt(unsigned char *key,
                                size_t key_len,
                                unsigned char *inData,
                                size_t inData_len, unsigned char **outData,
                                size_t * outData_len)
{
  // validate non-NULL and non-empty decryption key specified
  if (key == NULL || key_len == 0)
  {
    return 1;
  }

  // verify non-NULL and non-empty input ciphertext buffer of a valid length
  // (multiple of eight bytes greater than or equal to 8 bytes but less than
  // specification maximum)
  //
  // Note: 8 bytes (64 bits) is the size of a semiblock (half of the block
  //       size) for the AES codebook
  if (inData == NULL || inData_len == 0)
  {
    return 1;
  }
  if (inData_len < 8)
  {
    return 1;
  }
  if (inData_len % 8 != 0)
  {
    return 1;
  }
  if (inData_len > AES_KEYWRAP_5649PAD_MAX_DATA_LEN)
  {
    return 1;
  }

  // output data buffer (outData) will contain the decrypted plaintext, which
  // should be the same size as the input ciphertext data (original plaintext
  // plus prepended 4-byte integrity check value and 4-byte semiblock count
  // plus any appended padding bytes)
  *outData = NULL;
  *outData = malloc(inData_len);
  if (*outData == NULL)
  {
    return 1;
  }

  // OpenSSL requires WRAP_ALLOW flag be explicitly set
  // when using key wrap modes with EVP.
  EVP_CIPHER_CTX *ctx;

  if (!(ctx = EVP_CIPHER_CTX_new()))
  {
    free(*outData);
    return 1;
  }
  EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

  int init_result = 0;

  switch (key_len)
  {
  case 16:
    init_result =
      EVP_DecryptInit_ex(ctx, EVP_aes_128_wrap_pad(), NULL, NULL, NULL);
    break;
  case 24:
    init_result =
      EVP_DecryptInit_ex(ctx, EVP_aes_192_wrap_pad(), NULL, NULL, NULL);
    break;
  case 32:
    init_result =
      EVP_DecryptInit_ex(ctx, EVP_aes_256_wrap_pad(), NULL, NULL, NULL);
    break;
  default:
    break;
  }

  if (!init_result)
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, NULL))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  int tmp_len = 0;

  if (!EVP_DecryptUpdate(ctx, *outData, &tmp_len, inData, inData_len))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  *outData_len = tmp_len;
  if (!EVP_DecryptFinal_ex(ctx, *outData + *outData_len, &tmp_len))
  {
    free(*outData);
    EVP_CIPHER_CTX_free(ctx);
    return 1;
  }

  *outData_len += tmp_len;

  EVP_CIPHER_CTX_free(ctx);

  return 0;
}
