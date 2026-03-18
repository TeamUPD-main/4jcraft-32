#include "../Platform/stdafx.h"
#include <cstring>
#include "../../Minecraft.World/IO/Streams/FloatBuffer.h"
#include "Frustum.h"

Frustum* Frustum::frustum = new Frustum();

Frustum::Frustum() {
    _proj = MemoryTracker::createFloatBuffer(16);
    _modl = MemoryTracker::createFloatBuffer(16);
    _clip = MemoryTracker::createFloatBuffer(16);
}

Frustum::~Frustum() {
    delete _proj;
    delete _modl;
    delete _clip;
}

FrustumData* Frustum::getFrustum() {
    frustum->calculateFrustum();
    return frustum;
}

///////////////////////////////// NORMALIZE PLANE
///\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*
/////
/////	This normalizes a plane (A side) from a given frustum.
/////
///////////////////////////////// NORMALIZE PLANE
///\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*

void Frustum::normalizePlane(float** frustum, int side) {
    float magnitude = (float)sqrt(frustum[side][A] * frustum[side][A] +
                                  frustum[side][B] * frustum[side][B] +
                                  frustum[side][C] * frustum[side][C]);
    frustum[side][A] /= magnitude;
    frustum[side][B] /= magnitude;
    frustum[side][C] /= magnitude;
    frustum[side][D] /= magnitude;
}

void Frustum::calculateFrustum() {
    // Listening to Iyowa and reading this mess is a fever dream.
    {
        const float* p = RenderManager.MatrixGet(GL_PROJECTION_MATRIX);
        if (p) memcpy(&proj[0], p, 16 * sizeof(float));
    }
    {
        const float* m = RenderManager.MatrixGet(GL_MODELVIEW_MATRIX);
        if (m) memcpy(&modl[0], m, 16 * sizeof(float));
    }

    // Multiply modelview * projection to get the clip matrix.
    clip[0] = modl[0] * proj[0] + modl[1] * proj[4] + modl[2] * proj[8] +
              modl[3] * proj[12];
    clip[1] = modl[0] * proj[1] + modl[1] * proj[5] + modl[2] * proj[9] +
              modl[3] * proj[13];
    clip[2] = modl[0] * proj[2] + modl[1] * proj[6] + modl[2] * proj[10] +
              modl[3] * proj[14];
    clip[3] = modl[0] * proj[3] + modl[1] * proj[7] + modl[2] * proj[11] +
              modl[3] * proj[15];

    clip[4] = modl[4] * proj[0] + modl[5] * proj[4] + modl[6] * proj[8] +
              modl[7] * proj[12];
    clip[5] = modl[4] * proj[1] + modl[5] * proj[5] + modl[6] * proj[9] +
              modl[7] * proj[13];
    clip[6] = modl[4] * proj[2] + modl[5] * proj[6] + modl[6] * proj[10] +
              modl[7] * proj[14];
    clip[7] = modl[4] * proj[3] + modl[5] * proj[7] + modl[6] * proj[11] +
              modl[7] * proj[15];

    clip[8] = modl[8] * proj[0] + modl[9] * proj[4] + modl[10] * proj[8] +
              modl[11] * proj[12];
    clip[9] = modl[8] * proj[1] + modl[9] * proj[5] + modl[10] * proj[9] +
              modl[11] * proj[13];
    clip[10] = modl[8] * proj[2] + modl[9] * proj[6] + modl[10] * proj[10] +
               modl[11] * proj[14];
    clip[11] = modl[8] * proj[3] + modl[9] * proj[7] + modl[10] * proj[11] +
               modl[11] * proj[15];

    clip[12] = modl[12] * proj[0] + modl[13] * proj[4] + modl[14] * proj[8] +
               modl[15] * proj[12];
    clip[13] = modl[12] * proj[1] + modl[13] * proj[5] + modl[14] * proj[9] +
               modl[15] * proj[13];
    clip[14] = modl[12] * proj[2] + modl[13] * proj[6] + modl[14] * proj[10] +
               modl[15] * proj[14];
    clip[15] = modl[12] * proj[3] + modl[13] * proj[7] + modl[14] * proj[11] +
               modl[15] * proj[15];

    // Extract frustum planes from the clip matrix.

    // RIGHT
    m_Frustum[RIGHT][A] = clip[3] - clip[0];
    m_Frustum[RIGHT][B] = clip[7] - clip[4];
    m_Frustum[RIGHT][C] = clip[11] - clip[8];
    m_Frustum[RIGHT][D] = clip[15] - clip[12];
    normalizePlane(m_Frustum, RIGHT);

    // LEFT
    m_Frustum[LEFT][A] = clip[3] + clip[0];
    m_Frustum[LEFT][B] = clip[7] + clip[4];
    m_Frustum[LEFT][C] = clip[11] + clip[8];
    m_Frustum[LEFT][D] = clip[15] + clip[12];
    normalizePlane(m_Frustum, LEFT);

    // BOTTOM
    m_Frustum[BOTTOM][A] = clip[3] + clip[1];
    m_Frustum[BOTTOM][B] = clip[7] + clip[5];
    m_Frustum[BOTTOM][C] = clip[11] + clip[9];
    m_Frustum[BOTTOM][D] = clip[15] + clip[13];
    normalizePlane(m_Frustum, BOTTOM);

    // TOP
    m_Frustum[TOP][A] = clip[3] - clip[1];
    m_Frustum[TOP][B] = clip[7] - clip[5];
    m_Frustum[TOP][C] = clip[11] - clip[9];
    m_Frustum[TOP][D] = clip[15] - clip[13];
    normalizePlane(m_Frustum, TOP);

    // BACK
    m_Frustum[BACK][A] = clip[3] - clip[2];
    m_Frustum[BACK][B] = clip[7] - clip[6];
    m_Frustum[BACK][C] = clip[11] - clip[10];
    m_Frustum[BACK][D] = clip[15] - clip[14];
    normalizePlane(m_Frustum, BACK);

    // FRONT
    m_Frustum[FRONT][A] = clip[3] + clip[2];
    m_Frustum[FRONT][B] = clip[7] + clip[6];
    m_Frustum[FRONT][C] = clip[11] + clip[10];
    m_Frustum[FRONT][D] = clip[15] + clip[14];
    normalizePlane(m_Frustum, FRONT);
}