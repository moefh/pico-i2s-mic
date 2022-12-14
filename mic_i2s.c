/**
 * Record from an I2S microphone.
 *
 * This code was designed for the MSM261S4030H0 microphone, but should
 * be usable with some modifications with most I2S microphones (see
 * the PIO program to change the clock edge where the I2S data is
 * read, etc.).
 *
 * Used RP2040 resources:
 * - DMA IRQ 0 (exclusively)
 * - 1 dma channel
 * - 1 state machine from the selected PIO
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "mic_i2s.h"
#include "mic_i2s.pio.h"

static PIO mic_pio;
static uint mic_pio_sm;
static uint mic_dma_chan;
static uint mic_sample_freq;
static size_t mic_num_samples;

static volatile uint mic_cur_buffer_num;
static uint32_t *mic_sample_buffers[2];

static void __isr __time_critical_func(dma_handler)(void)
{
  // swap buffers
  uint cur_buf = !mic_cur_buffer_num;
  mic_cur_buffer_num = cur_buf;

  // set dma dest to new buffer and re-trigger dma:
  dma_hw->ch[mic_dma_chan].al2_write_addr_trig = (uintptr_t) mic_sample_buffers[cur_buf];

  // ack dma irq
  dma_hw->ints0 = 1u << mic_dma_chan;
}

int mic_i2s_init(uint pio_num, uint data_pin, uint sck_pin, uint sample_freq, size_t num_samples)
{
  // allocate buffer samples
  mic_sample_buffers[0] = malloc(num_samples * sizeof(uint32_t));
  mic_sample_buffers[1] = malloc(num_samples * sizeof(uint32_t));
  if (mic_sample_buffers[0] == NULL || mic_sample_buffers[1] == NULL) {
    free(mic_sample_buffers[0]);
    free(mic_sample_buffers[1]);
    return -1;
  }
  memset(mic_sample_buffers[0], 0, num_samples * sizeof(uint32_t));
  memset(mic_sample_buffers[1], 0, num_samples * sizeof(uint32_t));
  
  // setup pio
  mic_pio = (pio_num == 0) ? pio0 : pio1;
  uint offset = pio_add_program(mic_pio, &mic_i2s_program);
  mic_pio_sm = pio_claim_unused_sm(mic_pio, true);
  mic_i2s_program_init(mic_pio, mic_pio_sm, offset, sample_freq, data_pin, sck_pin);

  // allocate dma channel and setup irq
  mic_dma_chan = dma_claim_unused_channel(true);
  dma_channel_set_irq0_enabled(mic_dma_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
  irq_set_priority(DMA_IRQ_0, 0xff);
  irq_set_enabled(DMA_IRQ_0, true);

  mic_sample_freq = sample_freq;
  mic_num_samples = num_samples;
  return 0;
}

void mic_i2s_start(void)
{
  mic_cur_buffer_num = 0;
  uint32_t *buffer = mic_sample_buffers[mic_cur_buffer_num];

  // setup dma channel
  dma_channel_config dma_cfg = dma_channel_get_default_config(mic_dma_chan);
  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_cfg, false);
  channel_config_set_write_increment(&dma_cfg, true);
  channel_config_set_dreq(&dma_cfg, pio_get_dreq(mic_pio, mic_pio_sm, false));
  dma_channel_configure(mic_dma_chan, &dma_cfg,
                        buffer,                     // destination
                        &mic_pio->rxf[mic_pio_sm],  // source
                        mic_num_samples,            // number of transfers
                        true                        // start now (will be blocked by pio)
                        );

  // reset and start pio
  pio_sm_clear_fifos(mic_pio, mic_pio_sm);
  pio_sm_restart(mic_pio, mic_pio_sm);
  pio_sm_set_enabled(mic_pio, mic_pio_sm, true);
}

void mic_i2s_stop(void)
{
  pio_sm_set_enabled(mic_pio, mic_pio_sm, false);
  dma_channel_abort(mic_dma_chan);
}

uint32_t *mic_i2s_get_sample_buffer(bool block)
{
  uint buffer_num = mic_cur_buffer_num;
  if (block) {
    // wait until the IRQ changes the active buffer
    while (buffer_num == mic_cur_buffer_num)
      ;
  } else {
    // return the non-active buffer immediatelly
    buffer_num = !buffer_num;
  }
  return mic_sample_buffers[buffer_num];
}

uint32_t *mic_i2s_record_blocking(void)
{
  uint32_t *buffer = mic_sample_buffers[0];
  mic_i2s_record_buffer_blocking(buffer, mic_num_samples);
  return buffer;
}

void mic_i2s_record_buffer_blocking(uint32_t *buffer, size_t num_samples)
{
  pio_sm_clear_fifos(mic_pio, mic_pio_sm);
  pio_sm_restart(mic_pio, mic_pio_sm);
  pio_sm_set_enabled(mic_pio, mic_pio_sm, true);
  for (size_t i = 0; i < num_samples; i++) {
    buffer[i] = pio_sm_get_blocking(mic_pio, mic_pio_sm);
  }
  pio_sm_set_enabled(mic_pio, mic_pio_sm, false);
}
