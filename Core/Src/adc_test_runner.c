#include "adc_test_runner.h"

#include <string.h>

#include "adc0832_driver.h"
#include "adc_static_calc.h"
#include "dac.h"
#include "main.h"
#include "stm32_adc_monitor.h"

#define ADC_TEST_DAC_SETTLE_DELAY_MS 5U
#define ADC_TEST_DUT_AVERAGE_COUNT 8U

static AdcTestStatus g_status;
static uint32_t g_sample_index;
static uint32_t g_start_tick;

static void SetDacMv(uint32_t mv)
{
    if (mv > ADC_TEST_FULL_SCALE_MV) {
        mv = ADC_TEST_FULL_SCALE_MV;
    }

    const uint32_t dac_code =
        (mv * 4095U + (ADC_TEST_FULL_SCALE_MV / 2U)) / ADC_TEST_FULL_SCALE_MV;

    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_code);
}

static void InitStatusDefaults(void)
{
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = ADC_TEST_STATE_IDLE;
    g_status.mode = ADC_TEST_MODE_STATIC_8BIT;
    g_status.source = ADC_TEST_SOURCE_STM32_DAC;
    g_status.monitor_ok = 1U;
    g_status.total_samples = ADC_TEST_SAMPLE_COUNT_8;
    g_status.dut_adc_bits = ADC_TEST_BITS_8;
    g_status.snr_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    g_status.sinad_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    g_status.enob_x100 = ADC_TEST_DYNAMIC_INVALID;
    g_status.sfdr_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    g_status.thd_db_x100 = ADC_TEST_DYNAMIC_INVALID;
}

static void ResetScanData(void)
{
    const uint32_t mode = g_status.mode;
    const uint32_t source = g_status.source;

    InitStatusDefaults();
    g_status.mode = mode;
    g_status.source = source;

    g_sample_index = 0U;
    g_start_tick = HAL_GetTick();
    AdcStaticCalc_Init();
}

static void UpdateIdleMonitor(void)
{
    uint32_t raw12 = 0U;
    uint32_t mv = 0U;

    if (Stm32AdcMonitor_Read(&raw12, &mv)) {
        g_status.monitor_ok = 1U;
        g_status.input_mv = mv;
        g_status.stm32_adc_raw12 = raw12;
        g_status.stm32_adc_mv = mv;
    } else {
        g_status.monitor_ok = 0U;
    }
}

static void AcquireScanPoint(void)
{
    const uint32_t index = g_sample_index;
    const uint32_t target_mv =
        (index * ADC_TEST_FULL_SCALE_MV) / (ADC_TEST_SAMPLE_COUNT_8 - 1U);

    if (g_status.source == ADC_TEST_SOURCE_STM32_DAC) {
        SetDacMv(target_mv);
    }

    HAL_Delay(ADC_TEST_DAC_SETTLE_DELAY_MS);

    uint32_t raw_sum = 0U;
    uint32_t time_sum_ns = 0U;
    for (uint32_t i = 0U; i < ADC_TEST_DUT_AVERAGE_COUNT; ++i) {
        uint32_t raw = 0U;
        uint32_t one_time_ns = 0U;
        if (!Adc0832Driver_ReadCh0(&raw, &one_time_ns)) {
            g_status.state = ADC_TEST_STATE_ERROR;
            return;
        }

        raw_sum += raw;
        time_sum_ns += one_time_ns;
    }

    uint32_t stm32_adc_raw12 = 0U;
    uint32_t stm32_adc_mv = 0U;
    if (!Stm32AdcMonitor_Read(&stm32_adc_raw12, &stm32_adc_mv)) {
        g_status.monitor_ok = 0U;
        g_status.state = ADC_TEST_STATE_ERROR;
        return;
    }

    const uint32_t dut_adc_avg_x1000 = (raw_sum * 1000U) / ADC_TEST_DUT_AVERAGE_COUNT;
    const uint32_t dut_adc_code = (dut_adc_avg_x1000 + 500U) / 1000U;
    const uint32_t dut_conversion_time_ns = time_sum_ns / ADC_TEST_DUT_AVERAGE_COUNT;

    g_status.monitor_ok = 1U;
    g_status.elapsed_ms = HAL_GetTick() - g_start_tick;
    g_status.sample_index = index;
    g_status.total_samples = ADC_TEST_SAMPLE_COUNT_8;
    g_status.input_mv = stm32_adc_mv;
    g_status.dut_adc_code = dut_adc_code;
    g_status.dut_adc_bits = ADC_TEST_BITS_8;
    g_status.progress_permille = (index * 1000U) / (ADC_TEST_SAMPLE_COUNT_8 - 1U);
    g_status.dut_adc_avg_x1000 = dut_adc_avg_x1000;
    g_status.dut_conversion_time_ns = dut_conversion_time_ns;
    g_status.stm32_adc_raw12 = stm32_adc_raw12;
    g_status.stm32_adc_mv = stm32_adc_mv;

    AdcStaticCalc_Update(index, dut_adc_code, dut_conversion_time_ns, &g_status);

    ++g_sample_index;
    if (g_sample_index >= ADC_TEST_SAMPLE_COUNT_8) {
        g_status.progress_permille = 1000U;
        g_status.state = ADC_TEST_STATE_CALCULATING;
    }
}

void AdcTestRunner_Init(void)
{
    HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    Adc0832Driver_Init();
    Stm32AdcMonitor_Init();
    AdcStaticCalc_Init();

    g_sample_index = 0U;
    g_start_tick = HAL_GetTick();
    InitStatusDefaults();
    SetDacMv(0U);
}

void AdcTestRunner_HandleCommand(const AdcControlCommand *cmd)
{
    if ((cmd == NULL) || (cmd->cmd == ADC_TEST_CMD_NONE)) {
        return;
    }

    switch (cmd->cmd) {
    case ADC_TEST_CMD_START:
        ResetScanData();
        g_status.state = ADC_TEST_STATE_SCANNING;
        break;

    case ADC_TEST_CMD_STOP:
        g_status.state = ADC_TEST_STATE_STOPPED;
        break;

    case ADC_TEST_CMD_RESET:
        ResetScanData();
        g_status.state = ADC_TEST_STATE_IDLE;
        SetDacMv(0U);
        break;

    case ADC_TEST_CMD_SET_MODE:
        if (cmd->arg0 <= ADC_TEST_MODE_DYNAMIC) {
            g_status.mode = cmd->arg0;
            ResetScanData();
            g_status.mode = cmd->arg0;
            g_status.state = ADC_TEST_STATE_IDLE;
        }
        break;

    case ADC_TEST_CMD_SET_SOURCE:
        if (cmd->arg0 <= ADC_TEST_SOURCE_STM32_DAC) {
            g_status.source = cmd->arg0;
            ResetScanData();
            g_status.source = cmd->arg0;
            g_status.state = ADC_TEST_STATE_IDLE;
        }
        break;

    default:
        break;
    }
}

void AdcTestRunner_Task(AdcTestStatus *status)
{
    if (g_status.state == ADC_TEST_STATE_SCANNING) {
        AcquireScanPoint();
    } else if (g_status.state == ADC_TEST_STATE_CALCULATING) {
        g_status.elapsed_ms = HAL_GetTick() - g_start_tick;
        g_status.state = ADC_TEST_STATE_DONE;
    } else {
        UpdateIdleMonitor();
    }

    if (status != NULL) {
        *status = g_status;
    }
}
