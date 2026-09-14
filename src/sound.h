/* Sound commands for the nullsound Z80 driver. The command numbers come from the generated
 * sound_ids.h (tools/make_audio.py writes it together with the sample map). */
#ifndef SOUND_H
#define SOUND_H

#include <ngdevkit/types.h>
#include "sound_ids.h"

void sound_init(void);
/* queued: the Z80 takes one command per frame */
void sound_play(u8 command);
void sound_music(u8 command);
void sound_frame(void);

#endif
