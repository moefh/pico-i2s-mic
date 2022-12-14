#ifndef MIC_I2S_H_FILE
#define MIC_I2S_H_FILE

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize the PIO/DMA/IRQ for recording.
 *
 * The mic WS (word select) pin number must be immediately above the SCK
 * (clock) pin number for the pio program to work.
 *
 * This function allocates two buffers of `num_samples` samples for a
 * total of `8*num_samples` bytes.
 *
 * Return 0 on success, -1 on error (e.g. out of memory).
 */
int mic_i2s_init(uint pio_num, uint data_pin, uint sck_pin, uint sample_freq, size_t num_samples);

/**
 * Start asynchrnous recording via DMA/IRQ.
 *
 * Samples are written to a double buffer. Use
 * `mic_i2s_get_sample_buffer()` to get access to it.
 */
void mic_i2s_start(void);

/**
 * Stop asynchronous recording.
 *
 * This function must be called only if an asynchronous recording is
 * active.
 */
void mic_i2s_stop(void);

/**
 * Wait for one buffer's worth of samples.
 *
 * This function must be called during asynchronous recording (i.e.,
 * after calling `mic_i2s_start()` but before calling
 * `mic_i2s_stop()`).
 * 
 * If `block` is true, this function will wait until the current
 * buffer is finished recording before returning.  This is the
 * recommended way of using this function, otherwise the returned
 * buffer could immediatelly overwritten with new data.  Either way,
 * you should copy the buffer data to another location as soon as
 * possible, as its data *will* be eventually overwritten.
 *
 * Return a pointer to the last fully recorded buffer (or a buffer of
 * 0s if recodring was never completed).
 */
uint32_t *mic_i2s_get_sample_buffer(bool block);

/**
 * Record one buffer's worth of samples.
 *
 * This function must NOT be called during asynchronous recording.
 * This function blocks during recording.
 *
 * Return a pointer to the recorded buffer.
 */
uint32_t *mic_i2s_record_blocking(void);

/**
 * Record the given number of samples into the given buffer.
 *
 * This function must NOT be called during asynchronous recording.
 * This function blocks during recording.
 */
void mic_i2s_record_buffer_blocking(uint32_t *buffer, size_t num_samples);

#endif /* MIC_I2S_H_FILE */
