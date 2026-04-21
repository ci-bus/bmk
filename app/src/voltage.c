#include <hal/nrf_uicr.h>
#include <hal/nrf_nvmc.h>
#include <cmsis_core.h>

static int set_vdd_3v(void)
{
    uint32_t regout0 =
        (UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos);

    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) != regout0) {

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

        NRF_UICR->REGOUT0 = regout0;

        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;

        NVIC_SystemReset();
    }

    return 0;
}

SYS_INIT(set_vdd_3v, PRE_KERNEL_1, 0);