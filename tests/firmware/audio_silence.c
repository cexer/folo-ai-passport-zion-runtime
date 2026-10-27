/* Device-test safety profile: retain decoding and I2S traffic, silence output.
 * Never link this into a normal release. No application changes required. */
#include "esp_codec_dev.h"
#include "driver/i2s_std.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include "esp_log.h"
#include <stdint.h>

extern int __real_esp_codec_dev_open(esp_codec_dev_handle_t, esp_codec_dev_sample_info_t *);
extern int __real_esp_codec_dev_write(esp_codec_dev_handle_t, void *, int);
extern int __real_esp_codec_dev_set_out_vol(esp_codec_dev_handle_t, int);
extern int __real_esp_codec_dev_set_out_mute(esp_codec_dev_handle_t, bool);
extern esp_err_t __real_i2s_channel_write(i2s_chan_handle_t, const void *, size_t, size_t *, uint32_t);

static void silence_codec(esp_codec_dev_handle_t codec) {
    int volume = __real_esp_codec_dev_set_out_vol(codec, 0);
    int mute = __real_esp_codec_dev_set_out_mute(codec, true);
    static bool reported;
    if (!reported && !volume && !mute) {
        reported = true;
        ESP_LOGI("silent-test", "CODEC_MUTED volume=0 mute=1");
    }
}
int __wrap_esp_codec_dev_open(esp_codec_dev_handle_t codec, esp_codec_dev_sample_info_t *info) {
    int result = __real_esp_codec_dev_open(codec, info);
    if (!result) silence_codec(codec);
    return result;
}
int __wrap_esp_codec_dev_write(esp_codec_dev_handle_t codec, void *data, int length) {
    /* Open/setter hooks retain volume zero and mute. Reasserting hardware
     * registers for every PCM block needlessly contends with UI battery I2C.
     * The final I2S sink independently replaces every output byte with zero. */
    return __real_esp_codec_dev_write(codec, data, length);
}
int __wrap_esp_codec_dev_set_out_vol(esp_codec_dev_handle_t codec, int volume) {
    (void)volume;
    return __real_esp_codec_dev_set_out_vol(codec, 0);
}
int __wrap_esp_codec_dev_set_out_mute(esp_codec_dev_handle_t codec, bool mute) {
    (void)mute;
    return __real_esp_codec_dev_set_out_mute(codec, true);
}
esp_err_t __wrap_i2s_channel_write(i2s_chan_handle_t handle, const void *data,
                                  size_t length, size_t *written, uint32_t timeout_ms) {
    static const uint8_t zeros[256];
    if (!data || !length) return __real_i2s_channel_write(handle, data, length, written, timeout_ms);
    size_t total = 0;
    esp_err_t result = ESP_OK;
    int64_t start = esp_timer_get_time();
    while (total < length) {
        size_t part = length - total, sent = 0;
        if (part > sizeof(zeros)) part = sizeof(zeros);
        uint32_t remaining = timeout_ms;
        if (timeout_ms != UINT32_MAX) {
            uint64_t elapsed = (esp_timer_get_time() - start) / 1000;
            remaining = elapsed >= timeout_ms ? 0 : timeout_ms - (uint32_t)elapsed;
        }
        result = __real_i2s_channel_write(handle, zeros, part, &sent, remaining);
        total += sent;
        if (result != ESP_OK || sent != part) break;
    }
    if (written) *written = total;
    static atomic_uint zero_bytes;
    unsigned previous=atomic_fetch_add(&zero_bytes,(unsigned)total);
    if ((previous>>16)!=((previous+(unsigned)total)>>16))
        ESP_LOGI("silent-test", "I2S_ZERO_PROGRESS bytes=%u",previous+(unsigned)total);
    static bool reported;
    if (!reported && total) {
        reported = true;
        ESP_LOGI("silent-test", "I2S_ZERO_OUTPUT bytes=%u", (unsigned)total);
    }
    return result;
}
