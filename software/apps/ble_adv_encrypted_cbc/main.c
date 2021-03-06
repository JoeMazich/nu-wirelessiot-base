// BLE Advertisement Raw app
//
// Sends a BLE advertisement with raw bytes

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>

#include "simple_ble.h"

#include "nrf52840dk.h"

#include "nrf.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"

#include "nrf_drv_power.h"

#include "app_error.h"
#include "app_util.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "boards.h"

#include "nrf_crypto.h"
#include "nrf_crypto_error.h"
#include "mem_manager.h"

#if NRF_MODULE_ENABLED(NRF_CRYPTO)

#define AES_MAC_SIZE                            (16)

#define NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE    (100)

#define AES_ERROR_CHECK(error)  \
    do {            \
        if (error)  \
        {           \
            NRF_LOG_RAW_INFO("\r\nError = 0x%x\r\n%s\r\n",           \
                             (error),                                \
                             nrf_crypto_error_string_get(error));    \
            return; \
        }           \
    } while (0);

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:4e:xx:xx
        .platform_id       = 0x4E,   // used as 4th octect in device BLE address
        .device_id         = 0xAABB, // must be unique on each device you program!
        .adv_name          = "BLE Crypto", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};


/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
simple_ble_app_t* simple_ble_app;

// 64 bit
static uint8_t m_key[8] = {'W', 'A', 'S', 'S', 'U', 'P', 'P', 'P'};

// 128 bit
static uint8_t n_key[16] = {'C', 'A', 'N', ' ', 'W', 'E', ' ', 'G',
                            'E', 'T', ' ', 'A', 'N', ' ', 'A', '?'};

static char m_plain_text[NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE] =
{
    "BLECrypto is fun"
};

static void text_print(char const* p_label, char const * p_text, size_t len)
{
    NRF_LOG_RAW_INFO("----%s (length: %u) ----\r\n", p_label, len);
    NRF_LOG_FLUSH();
    for(size_t i = 0; i < len; i++)
    {
        NRF_LOG_RAW_INFO("%c", p_text[i]);
        NRF_LOG_FLUSH();
    }
    NRF_LOG_RAW_INFO("\r\n");
    NRF_LOG_RAW_INFO("---- %s end ----\r\n\r\n", p_label);
    NRF_LOG_FLUSH();
}

static void hex_text_print(char const* p_label, char const * p_text, size_t len)
{
    NRF_LOG_RAW_INFO("---- %s (length: %u) ----\r\n", p_label, len);
    NRF_LOG_FLUSH();

    // Handle partial line (left)
    for (size_t i = 0; i < len; i++)
    {
        if (((i & 0xF) == 0) && (i > 0))
        {
            NRF_LOG_RAW_INFO("\r\n");
            NRF_LOG_FLUSH();
        }

        NRF_LOG_RAW_INFO("%02x ", p_text[i]);
        NRF_LOG_FLUSH();
    }
    NRF_LOG_RAW_INFO("\r\n");
    NRF_LOG_RAW_INFO("---- %s end ----\r\n\r\n", p_label);
    NRF_LOG_FLUSH();
}

static void plain_text_print(void)
{
    size_t len = strlen(m_plain_text);

    text_print("Plain text", m_plain_text, len);
    hex_text_print("Plain text (hex)", m_plain_text, len);
}

static void mac_print(uint8_t const * p_buff, uint8_t mac_size)
{
    hex_text_print("Advertising MAC (hex)", (char const *)p_buff, mac_size);
}

int main(void) {

  printf("Board started. Initializing BLE: \n");

  ret_code_t ret;

  APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  NRF_LOG_RAW_INFO("AES CBC_MAC example started.\r\n\r\n");
  NRF_LOG_FLUSH();

  ret = nrf_drv_clock_init();
  APP_ERROR_CHECK(ret);
  nrf_drv_clock_lfclk_request(NULL);

  ret = nrf_crypto_init();
  APP_ERROR_CHECK(ret);

  ret = nrf_mem_init();
  APP_ERROR_CHECK(ret);

  uint8_t     iv[NRF_CRYPTO_MBEDTLS_AES_IV_SIZE];
  uint8_t     mac[AES_MAC_SIZE];
  ret_code_t  ret_val;
  size_t      len_in;
  size_t      len_out;

  nrf_crypto_aes_context_t cbc_mac_128_ctx;

  memset(mac, 0, sizeof(mac));

  plain_text_print();

  /* Init cbc_mac context */
  ret_val = nrf_crypto_aes_init(&cbc_mac_128_ctx,
                                &g_nrf_crypto_aes_cbc_mac_128_info,
                                NRF_CRYPTO_MAC_CALCULATE);
  //AES_ERROR_CHECK(ret_val);

  /* Set encryption key */
  ret_val = nrf_crypto_aes_key_set(&cbc_mac_128_ctx, n_key);
  //AES_ERROR_CHECK(ret_val);

  /* Set IV */
  memset(iv, 0, sizeof(iv));

  ret_val = nrf_crypto_aes_iv_set(&cbc_mac_128_ctx, iv);
  //AES_ERROR_CHECK(ret_val);

  len_in  = strlen(m_plain_text);
  len_out = sizeof(mac);

  /* Calculate MAC */
  ret_val = nrf_crypto_aes_finalize(&cbc_mac_128_ctx,
                                    (uint8_t *)m_plain_text,
                                    len_in,
                                    (uint8_t *)mac,
                                    &len_out);
  //AES_ERROR_CHECK(ret_val);

  ret_val = nrf_crypto_aes_uninit(&cbc_mac_128_ctx);
  //AES_ERROR_CHECK(ret_val);

  mac_print(mac, len_out);

  // Setup BLE
  // Note: simple BLE is our own library. You can find it in `nrf5x-base/lib/simple_ble/`
  simple_ble_app = simple_ble_init(&ble_config);

  // Start Advertising
  uint8_t header[] = {0x02, 0x01, 0x06, 0x11, 0x09};
  uint8_t ble_data[21];

  int i;
  for (i=0; i<=4; i++)
  {
    ble_data[i] = header[i];
  }
  for (i=5; i<=21; i++)
  {
    ble_data[i] = mac[i - 5];
  }



  //uint8_t ble_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX] = {0x02, 0x01, 0x06, 0x11, 0x09, 0x42, 0x4c, 0x45, 0x43, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x20, 0x69, 0x73, 0x20, 0x66, 0x75, 0x6e, };

  simple_ble_adv_raw(ble_data, 21);
  printf("Started BLE advertisements\n");

  while(1)
  {
      NRF_LOG_FLUSH();
      UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
  }
}

#endif
