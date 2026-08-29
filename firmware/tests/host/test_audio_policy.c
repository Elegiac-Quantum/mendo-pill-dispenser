#include <assert.h>
#include "pill_audio/player.h"

int main(void)
{
    assert(pill_audio_test_policy_valid(35, 500, 8192));
    assert(!pill_audio_test_policy_valid(36, 500, 8192));
    assert(!pill_audio_test_policy_valid(35, 501, 8192));
    assert(!pill_audio_test_policy_valid(35, 500, 8193));
    assert(pill_audio_path_valid("/sdcard/smartpill/audio/family.wav"));
    assert(!pill_audio_path_valid("family.wav"));
    assert(!pill_audio_path_valid("/sdcard/smartpill/audio/../secret.wav"));
    assert(!pill_audio_path_valid("/sdcard/smartpill/audio/family.mp3"));
    assert(!pill_audio_path_valid("/sdcard/smartpill/audio/.wav"));
    return 0;
}
