#pragma once

//@name glint.Vec2
typedef struct gVec2 {
    //@desc Description
    //@getset
    float x;
    //@getset
    float y;
} gVec2;

#ifdef __cplusplus
extern "C" {
#endif

//@func
//@name glint.Vec2.new
gVec2 gvec2_new(float x, float y);

//@method
//@name glint.Vec2:scale
void gvec2_scale(
    //@self
    gVec2 *vec,
    float x
);

//@metamethod
//@name glint.Vec2:__add
gVec2 gvec2_add(gVec2 a, gVec2 b);

#ifdef __cplusplus
}
#endif
