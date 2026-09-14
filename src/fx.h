/* Hit sparks, dust, super flash and projectiles (a small pool of animated sprites). */
#ifndef FX_H
#define FX_H

#include "fighter.h"
#include "fx_anims.h"   /* generated: FXA_* animation ids */

void fx_projectile(Fighter *owner, const Move *m);
/* a projectile of the opponent is flying toward `f` (lets holding back block it) */
u8 fx_threatens(const Fighter *f);
void fighter_take_projectile(Fighter *f, const Move *m, s16 from_x);
void fx_draw_single(u16 sprite, u8 anim, s16 x, s16 y);

#endif
