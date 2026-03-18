#include "../Platform/stdafx.h"
#include "Lighting.h"
#include "../../Minecraft.World/IO/Streams/FloatBuffer.h"
#include "../../Minecraft.World/Util/Vec3.h"

FloatBuffer* Lighting::lb = new FloatBuffer(16);

static const float DIFFUSE = 0.6f;
static const float AMBIENT = 0.4f;

// light 0 : Vec3(0.2, 1.0, -0.7).normalize()
static const float L0X = 0.173913f, L0Y = 0.869565f, L0Z = -0.608696f;
// light 1 : Vec3(-0.2, 1.0,  0.7).normalize()
static const float L1X = -0.173913f, L1Y = 0.869565f, L1Z = 0.608696f;
// its ugly

void Lighting::turnOff() { RenderManager.StateSetLightingEnable(false); }

void Lighting::turnOn() {
    RenderManager.StateSetLightDirection(0, L0X, L0Y, L0Z);
    RenderManager.StateSetLightDirection(1, L1X, L1Y, L1Z);
    RenderManager.StateSetLightColour(0, DIFFUSE, DIFFUSE, DIFFUSE);
    RenderManager.StateSetLightAmbientColour(AMBIENT, AMBIENT, AMBIENT);
    RenderManager.StateSetLightingEnable(true);

    // flat shading existed but now its def soooooooooooooooo
}

void Lighting::turnOnGui() {
    RenderManager.MatrixPush();

    // glRotatef(-30, 0, 1, 0)
    RenderManager.MatrixRotate(-30.f * (3.14159265f / 180.f), 0.f, 1.f, 0.f);
    // glRotatef(165, 1, 0, 0)
    RenderManager.MatrixRotate(165.f * (3.14159265f / 180.f), 1.f, 0.f, 0.f);

    turnOn();

    RenderManager.MatrixPop();
}

FloatBuffer* Lighting::getBuffer(double a, double b, double c, double d) {
    return getBuffer((float)a, (float)b, (float)c, (float)d);
}

FloatBuffer* Lighting::getBuffer(float a, float b, float c, float d) {
    lb->clear();
    lb->put(a)->put(b)->put(c)->put(d);
    lb->flip();
    return lb;
}