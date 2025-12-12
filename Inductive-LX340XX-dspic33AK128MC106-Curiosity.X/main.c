/*
 * Resolver-to-encoder bridge for dsPIC33AK128MC106 Curiosity.
 *
 * Samples two ADC2 channels (sine / cosine), normalises them with
 * configurable offsets and gains, computes the mechanical angle via
 * atan2f, and computes quadrature A/B/Z states in software (no GPIO drive).
 * All key variables are exposed to X2Cscope so the accompanying Python GUI
 * (ResolverEncoder.py) can calibrate offsets/gains, adjust counts per
 * revolution, and visualise the signals.
 */

#include "mcc_generated_files/X2Cscope/X2Cscope.h"
#include "mcc_generated_files/adc/adc2.h"
#include "mcc_generated_files/system/interrupt_types.h"
#include "mcc_generated_files/system/clock.h"
#include "mcc_generated_files/system/pins.h"
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/timer/tmr1.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef RESOLVER_PI_F
#define RESOLVER_PI_F     3.14159265358979323846f
#endif
#define RESOLVER_TWO_PI_F (2.0f * RESOLVER_PI_F)

/* -------------------------------------------------------------------------- */
/* Resolver / encoder state exposed to X2Cscope                               */
volatile uint16_t sin_raw = 0U;
volatile uint16_t cos_raw = 0U;

volatile float sin_offset = 2048.0f;
volatile float cos_offset = 2048.0f;
volatile float sin_amplitude = (1.0f / 2048.0f); /* gain to normalise to +/-1 */
volatile float cos_amplitude = (1.0f / 2048.0f);

volatile float sin_calibrated = 0.0f;
volatile float cos_calibrated = 0.0f;
volatile float resolver_position = 0.0f; /* radians, [-pi, pi] */
volatile float trig_calc_time_us = 0.0f;
volatile float abz_calc_time_us = 0.0f;

volatile uint8_t encoder_A = 0U;
volatile uint8_t encoder_B = 0U;
volatile uint8_t encoder_Z = 0U;
volatile uint16_t counts_per_rev = 500U;
volatile uint32_t sample_counter = 0UL;

/* -------------------------------------------------------------------------- */
/* Internal flags                                                             */
static volatile bool sin_sampled = false;
static volatile bool cos_sampled = false;
static volatile bool sample_ready = false;
static uint32_t g_timer_period_counts = 1UL;
static float g_timer_tick_us = 1.0f;

static inline float sanitize_gain(float gain)
{
    const float min_abs = 1.0e-6f;
    if ((gain > min_abs) || (gain < -min_abs))
    {
        return gain;
    }
    /* Avoid divide-by-zero if the GUI writes 0.0 */
    return (gain >= 0.0f) ? min_abs : -min_abs;
}

static inline float wrap_angle_positive(float angle)
{
    float wrapped = angle;
    if (wrapped >= RESOLVER_TWO_PI_F)
    {
        wrapped -= RESOLVER_TWO_PI_F;
    }
    else if (wrapped < 0.0f)
    {
        wrapped += RESOLVER_TWO_PI_F;
    }
    return wrapped;
}

static inline uint32_t tick_delta(uint32_t start, uint32_t end)
{
    if (end >= start)
    {
        return end - start;
    }
    return (g_timer_period_counts - start) + end;
}

static void start_conversions(void)
{
    /* Use the MCC-configured software trigger for the enabled channels. */
    ADC2_SoftwareTriggerDisable();
    ADC2_SoftwareTriggerEnable();
}

static void update_encoder_outputs(float angle)
{
    float angle_pos = wrap_angle_positive(angle);
    uint16_t cpr = counts_per_rev;
    if (cpr == 0U)
    {
        cpr = 1U; /* Prevent divide-by-zero */
        counts_per_rev = cpr;
    }

    const uint32_t total_states = (uint32_t)cpr * 4UL;
    uint32_t idx = (uint32_t)((angle_pos / RESOLVER_TWO_PI_F) * (float)total_states);
    if (idx >= total_states)
    {
        idx = total_states - 1UL;
    }

    const uint32_t quad_state = idx & 0x3UL;
    const uint32_t count = idx >> 2;

    const uint8_t a = (quad_state == 1U || quad_state == 2U) ? 1U : 0U;
    const uint8_t b = (quad_state >= 2U) ? 1U : 0U;
    const uint8_t z = (count == 0U) ? 1U : 0U;

    encoder_A = a;
    encoder_B = b;
    encoder_Z = z;
}

static void process_samples(void)
{
    if (!sample_ready)
    {
        return;
    }
    sample_ready = false;

    const float s_gain = sanitize_gain(sin_amplitude);
    const float c_gain = sanitize_gain(cos_amplitude);
    const uint32_t trig_start = TMR1;
    const float s = ((float)sin_raw - sin_offset) * s_gain;
    const float c = ((float)cos_raw - cos_offset) * c_gain;

    sin_calibrated = s;
    cos_calibrated = c;

    resolver_position = atan2f(s, c);
    const uint32_t trig_end = TMR1;
    trig_calc_time_us = (float)tick_delta(trig_start, trig_end) * g_timer_tick_us;

    const uint32_t abz_start = TMR1;
    update_encoder_outputs(resolver_position);
    const uint32_t abz_end = TMR1;
    abz_calc_time_us = (float)tick_delta(abz_start, abz_end) * g_timer_tick_us;

    X2CScope_Update();
}

static void App_Timer1TimeoutCallback(void)
{
    sample_counter++;
    start_conversions();
}

static void ADC_ConversionDone(enum ADC2_CHANNEL channel, uint16_t adcVal)
{
    if (channel == ADC2_Channel0)
    {
        sin_raw = adcVal;
        sin_sampled = true;
    }
    else if (channel == ADC2_Channel1)
    {
        cos_raw = adcVal;
        cos_sampled = true;
    }
    else
    {
        return;
    }

    if (sin_sampled && cos_sampled)
    {
        sin_sampled = false;
        cos_sampled = false;
        sample_ready = true;
    }
}

static void resolver_initialize(void)
{
    ADC2_ChannelCallbackRegister(ADC_ConversionDone);
    ADC2_IndividualChannelInterruptFlagClear(ADC2_Channel0);
    ADC2_IndividualChannelInterruptFlagClear(ADC2_Channel1);
    ADC2_IndividualChannelInterruptPrioritySet(ADC2_Channel0, INTERRUPT_PRIORITY_4);
    ADC2_IndividualChannelInterruptPrioritySet(ADC2_Channel1, INTERRUPT_PRIORITY_4);
    ADC2_IndividualChannelInterruptEnable(ADC2_Channel0);
    ADC2_IndividualChannelInterruptEnable(ADC2_Channel1);

    /* Move Timer1 interrupt to our handler (sampling cadence) */
    Timer1_TimeoutCallbackRegister(App_Timer1TimeoutCallback);

    /* Kick the first conversion pair in case Timer1 is not yet ticking. */
    start_conversions();
}

int main(void)
{
    SYSTEM_Initialize();
    g_timer_period_counts = (uint32_t)(PR1 + 1UL);
    if (g_timer_period_counts == 0UL)
    {
        g_timer_period_counts = 1UL;
    }
    const float fcy = (float)CLOCK_InstructionFrequencyGet();
    if (fcy > 0.0f)
    {
        g_timer_tick_us = 1000000.0f / fcy;
    }
    else
    {
        g_timer_tick_us = 0.01f;
    }
    resolver_initialize();

    while (1)
    {
        /* Ensure callbacks fire even if channel interrupts are masked */
        ADC2_ChannelTasks(ADC2_Channel0);
        ADC2_ChannelTasks(ADC2_Channel1);
        /* Failsafe: if we have not latched a pair yet, retrigger. */
        if (!sample_ready)
        {
            start_conversions();
        }
        process_samples();
        X2CScope_Update();
        X2CScope_Communicate();
    }
}
