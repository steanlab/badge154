#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "txpower.h"

/* index -> radio setting of the SDK (rf_drv.h) and its value in dBm (rounded; the exact values are in the comments).
 * The chip has no smooth scale, only this table of fixed settings. */
static const RF_PowerTypeDef LEVEL[TX_LEVELS] = {
    RF_POWER_P10p46dBm, /* 10.46 */
    RF_POWER_P7p02dBm,  /*  7.02 */
    RF_POWER_P3p01dBm,  /*  3.01  default */
    RF_POWER_P0p04dBm,  /*  0.04 */
    RF_POWER_N3p03dBm,  /* -3.03 */
    RF_POWER_N5p81dBm,  /* -5.81 */
    RF_POWER_N9p89dBm,  /* -9.89 */
    RF_POWER_N15p88dBm, /* -15.88 */
    RF_POWER_N19p27dBm, /* -19.27 */
};
static const int8_t DBM[TX_LEVELS] = {10, 7, 3, 0, -3, -6, -10, -16, -19};

RAM static uint8_t tx_idx = TX_DEFAULT; /* retention RAM: survives the sleep */

_attribute_ram_code_ void txpower_apply(void)
{
    rf_set_power_level_index(LEVEL[tx_idx]);
}

void txpower_set(uint8_t idx)
{
    if (idx >= TX_LEVELS)
        return;
    tx_idx = idx;
    txpower_apply();
}

int8_t txpower_dbm(void)
{
    return DBM[tx_idx];
}
