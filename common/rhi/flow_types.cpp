#include "flow_types.h"

/*** Preset Configurations ***/

struct Preset defaultYACReaderFlowConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    3.f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.01f, // View_rotate_add sets the speed of the rotation
    0.02f, // View_rotate_sub sets the speed of reversing the rotation
    20.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    0.f, // CF_Y the Y Position of the Coverflow
    -8.f, // CF_Z the Z Position of the Coverflow

    15.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    -50.f, // Rotation sets the rotation of each cover
    0.18f, // X_Distance sets the distance between the covers
    1.f, // Center_Distance sets the distance between the centered and the non centered covers
    0.1f, // Z_Distance sets the pushback amount
    0.0f, // Y_Distance sets the elevation amount

    30.f // zoom level

};

struct Preset presetYACReaderFlowClassicConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    2.f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.08f, // View_rotate_add sets the speed of the rotation
    0.08f, // View_rotate_sub sets the speed of reversing the rotation
    30.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.f, // CF_Z the Z Position of the Coverflow

    0.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    -40.f, // Rotation sets the rotation of each cover
    0.18f, // X_Distance sets the distance between the covers
    1.f, // Center_Distance sets the distance between the centered and the non centered covers
    0.1f, // Z_Distance sets the pushback amount
    0.0f, // Y_Distance sets the elevation amount

    22.f // zoom level

};

struct Preset presetYACReaderFlowStripeConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    6.f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    4.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.08f, // View_rotate_add sets the speed of the rotation
    0.08f, // View_rotate_sub sets the speed of reversing the rotation
    30.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.f, // CF_Z the Z Position of the Coverflow

    0.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    0.f, // Rotation sets the rotation of each cover
    1.1f, // X_Distance sets the distance between the covers
    0.2f, // Center_Distance sets the distance between the centered and the non centered covers
    0.01f, // Z_Distance sets the pushback amount
    0.0f, // Y_Distance sets the elevation amount

    22.f // zoom level

};

struct Preset presetYACReaderFlowOverlappedStripeConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    2.f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.08f, // View_rotate_add sets the speed of the rotation
    0.08f, // View_rotate_sub sets the speed of reversing the rotation
    30.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.f, // CF_Z the Z Position of the Coverflow

    0.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    0.f, // Rotation sets the rotation of each cover
    0.18f, // X_Distance sets the distance between the covers
    1.f, // Center_Distance sets the distance between the centered and the non centered covers
    0.1f, // Z_Distance sets the pushback amount
    0.0f, // Y_Distance sets the elevation amount

    22.f // zoom level

};

struct Preset pressetYACReaderFlowUpConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    2.5f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.08f, // View_rotate_add sets the speed of the rotation
    0.08f, // View_rotate_sub sets the speed of reversing the rotation
    5.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.f, // CF_Z the Z Position of the Coverflow

    0.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    -50.f, // Rotation sets the rotation of each cover
    0.18f, // X_Distance sets the distance between the covers
    1.f, // Center_Distance sets the distance between the centered and the non centered covers
    0.1f, // Z_Distance sets the pushback amount
    -0.1f, // Y_Distance sets the elevation amount

    22.f // zoom level

};

struct Preset pressetYACReaderFlowDownConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.5f, // Animation_speedup sets the acceleration of the animation
    0.1f, // Animation_step_max sets the maximum speed of the animation
    2.5f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.08f, // View_rotate_add sets the speed of the rotation
    0.08f, // View_rotate_sub sets the speed of reversing the rotation
    5.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.f, // CF_Z the Z Position of the Coverflow

    0.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    -50.f, // Rotation sets the rotation of each cover
    0.18f, // X_Distance sets the distance between the covers
    1.f, // Center_Distance sets the distance between the centered and the non centered covers
    0.1f, // Z_Distance sets the pushback amount
    0.1f, // Y_Distance sets the elevation amount

    22.f // zoom level
};

// A carousel: the covers wheel away from the centre instead of fanning past it. The
// pushback is what does the work - it is more than five times the default, so covers
// either side recede quickly and the row reads as curving around an axis rather than
// lying flat. The stronger per-cover rotation turns them to face that curve, and the
// longer fade distance keeps more of them in view so there is a wheel to see rather
// than three covers and darkness.
struct Preset presetYACReaderFlowCarouselConfig = {
    0.08f, // Animation_step sets the speed of the animation
    1.6f, // Animation_speedup sets the acceleration of the animation
    0.12f, // Animation_step_max sets the maximum speed of the animation
    6.f, // Animation_Fade_out_dis sets the distance of view

    1.5f, // pre_rotation sets the rotation increasion
    3.f, // View_rotate_light_strenght sets the light strenght on rotation
    0.01f, // View_rotate_add sets the speed of the rotation
    0.02f, // View_rotate_sub sets the speed of reversing the rotation
    26.f, // View_angle sets the maximum view angle

    0.f, // CF_X the X Position of the Coverflow
    -0.2f, // CF_Y the Y Position of the Coverflow
    -7.4f, // CF_Z the Z Position of the Coverflow

    8.f, // CF_RX the X Rotation of the Coverflow
    0.f, // CF_RY the Y Rotation of the Coverflow
    0.f, // CF_RZ the Z Rotation of the Coverflow

    -68.f, // Rotation sets the rotation of each cover
    0.145f, // X_Distance sets the distance between the covers
    0.95f, // Center_Distance sets the distance between the centered and the non centered covers
    0.55f, // Z_Distance sets the pushback amount
    0.f, // Y_Distance sets the elevation amount

    30.f // zoom level
};
