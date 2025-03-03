#ifndef _SRENDERER_COMMON_HIT_HEADER_
#define _SRENDERER_COMMON_HIT_HEADER_

#include "common_trace.h"
#include "../../../Utility/math.h"
#include "../../../Utility/geometry.h"

// This will store two of the barycentric coordinates of the intersection when
// closest-hit shaders are called:
hitAttributeEXT vec2 attributes;

// Hit geometry data to return
struct HitGeometry {
    vec3 worldPosition;
    uint matID;
    vec3 geometryNormal;
    uint lightID;
    vec2 uv;
    uint geometryID;
    uint padding;
    mat3 TBN;
    vec3 geometryNormalUnflipped;
};

HitGeometry getHitGeometry() {
    HitGeometry hit;
    // Get all ids
    const int primitiveID = gl_PrimitiveID;
    const int geometryID = gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT;
    const GeometryInfo geometryInfo = geometryInfos[geometryID];
    const mat4 o2w = ObjectToWorld(geometryInfo);
    const mat4 o2wn = ObjectToWorldNormal(geometryInfo);
    // Get matID
    hit.matID = geometryInfo.materialID;
    hit.geometryID = geometryID;
    hit.lightID = geometryInfo.lightID;
#if (PRIMITIVE_TYPE == PRIMITIVE_SPHERE)
    // ray data
    const vec3 ray_origin    = gl_WorldRayOriginEXT;
    const vec3 ray_direction = gl_WorldRayDirectionEXT;
    // Sphere data
    const vec3  sphere_center = (o2w * vec4(0,0,0,1)).xyz;
    const float sphere_radius = length((o2w * vec4(1,0,0,1)).xyz - sphere_center);
    // Record the intersection
    // *: we should re-normalize it, using tHit is super unstable
    const vec3 hitPoint = sphere_center + sphere_radius * normalize(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT - sphere_center);
    const vec3 geometric_normal = normalize(hitPoint - sphere_center);
    const vec3 cartesian = normalize((transpose(o2wn) * vec4(geometric_normal, 0)).xyz);
    // We use the spherical coordinates as uv
    // We use the convention that y is up axis.
    // https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
    const float elevation = acos(clamp(cartesian.y, -1., 1.));
    const float azimuth = atan2(cartesian.z, cartesian.x);
    hit.worldPosition = hitPoint;
    hit.uv = vec2(-azimuth * k_inv_2_pi, elevation * k_inv_pi);
    const vec3 wNormal = geometric_normal;
    // const vec3 wTangent = cross(geometric_normal, vec3(0,1,0));
    // vec3 wBitangent = cross(wNormal, wTangent) * geometryInfo.oddNegativeScaling;
    // hit.TBN = mat3(wTangent, wBitangent, wNormal);
    hit.TBN = createFrame(wNormal);
    hit.geometryNormalUnflipped = hit.TBN[2];
    hit.TBN[2] = faceforward(hit.TBN[2], gl_WorldRayDirectionEXT, hit.TBN[2]);
    hit.geometryNormal = hit.TBN[2]; // for sphere, geometry normal and shading normal are similar
#elif (PRIMITIVE_TYPE == PRIMITIVE_TRIANGLE)
    // Get the indices of the vertices of the triangle
    const uint i0 = indices[3 * primitiveID + 0 + geometryInfo.indexOffset];
    const uint i1 = indices[3 * primitiveID + 1 + geometryInfo.indexOffset];
    const uint i2 = indices[3 * primitiveID + 2 + geometryInfo.indexOffset];
    // Get the vertices of the triangle
    const vec3 v0 = vertices[i0 + geometryInfo.vertexOffset].position;
    const vec3 v1 = vertices[i1 + geometryInfo.vertexOffset].position;
    const vec3 v2 = vertices[i2 + geometryInfo.vertexOffset].position;
    // Get the barycentric coordinates of the intersection
    vec3 barycentrics = vec3(0.0, attributes.x, attributes.y);
    barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;
    // Get position
    vec3 position = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    vec4 positionWorld =  o2w * vec4(position, 1);
    hit.worldPosition = positionWorld.xyz;
    // Get texcoord
    const vec2 u0 = vertices[i0 + geometryInfo.vertexOffset].texCoords;
    const vec2 u1 = vertices[i1 + geometryInfo.vertexOffset].texCoords;
    const vec2 u2 = vertices[i2 + geometryInfo.vertexOffset].texCoords;
    hit.uv = u0 * barycentrics.x + u1 * barycentrics.y + u2 * barycentrics.z;
    // Get BTN
    const vec3 geometryNormal = normalize((o2wn * vec4(cross(v1 - v0, v2 - v0), 0)).xyz);
    vec3 n[3], t[3];
    n[0] = vertices[i0 + geometryInfo.vertexOffset].normal;
    n[1] = vertices[i1 + geometryInfo.vertexOffset].normal;
    n[2] = vertices[i2 + geometryInfo.vertexOffset].normal;
    t[0] = vertices[i0 + geometryInfo.vertexOffset].tangent;
    t[1] = vertices[i1 + geometryInfo.vertexOffset].tangent;
    t[2] = vertices[i2 + geometryInfo.vertexOffset].tangent;
    for(int i=0; i<3; ++i) {
        n[i] = normalize((o2wn * vec4(n[i], 0)).xyz);
        t[i] = normalize((o2w * vec4(t[i], 0)).xyz);
    }
    const vec3 normal = n[0] * barycentrics.x + n[1] * barycentrics.y + n[2] * barycentrics.z;
    const vec3 tangent = t[0] * barycentrics.x + t[1] * barycentrics.y + t[2] * barycentrics.z;
    hit.TBN = buildTangentToWorld(vec4(tangent, geometryInfo.oddNegativeScaling), normal);
    hit.TBN[2] = faceforward(hit.TBN[2], gl_WorldRayDirectionEXT, geometryNormal);
    hit.geometryNormalUnflipped = geometryNormal;
    hit.geometryNormal = faceforward(geometryNormal, gl_WorldRayDirectionEXT, geometryNormal);
#endif
    // Return hit geometry
    return hit;
}

// Hit geometry data to return
struct HitGeometryAlphaTest {
    vec2 uv;
    uint matID;
};

HitGeometryAlphaTest getHitGeometryAlphaTest() {
    HitGeometryAlphaTest hit;
    // Get all ids
    const int primitiveID = gl_PrimitiveID;
    const int geometryID = gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT;
    const GeometryInfo geometryInfo = geometryInfos[geometryID];
    // Get matID
    hit.matID = geometryInfo.materialID;
#if (PRIMITIVE_TYPE == PRIMITIVE_SPHERE)
    // ray data
    const vec3 ray_origin    = gl_WorldRayOriginEXT;
    const vec3 ray_direction = gl_WorldRayDirectionEXT;
    // Sphere data
    const mat4 o2w = ObjectToWorld(geometryInfo);
    const mat4 o2wn = ObjectToWorldNormal(geometryInfo);
    const vec3  sphere_center = (o2w * vec4(0,0,0,1)).xyz;
    const float sphere_radius = length((o2w * vec4(1,0,0,1)).xyz - sphere_center);
    // Record the intersection
    // *: we should re-normalize it, using tHit is super unstable
    const vec3 hitPoint = sphere_center + sphere_radius * normalize(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT - sphere_center);
    const vec3 geometric_normal = normalize(hitPoint - sphere_center);
    const vec3 cartesian = normalize((transpose(o2wn) * vec4(geometric_normal, 0)).xyz);
    // We use the spherical coordinates as uv
    // We use the convention that y is up axis.
    // https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
    const float elevation = acos(clamp(cartesian.y, -1., 1.));
    const float azimuth = atan2(cartesian.z, cartesian.x);
    hit.uv = vec2(-azimuth * k_inv_2_pi, elevation * k_inv_pi);
#elif (PRIMITIVE_TYPE == PRIMITIVE_TRIANGLE)
    // Get the indices of the vertices of the triangle
    const uint i0 = indices[3 * primitiveID + 0 + geometryInfo.indexOffset];
    const uint i1 = indices[3 * primitiveID + 1 + geometryInfo.indexOffset];
    const uint i2 = indices[3 * primitiveID + 2 + geometryInfo.indexOffset];
    // Get the vertices of the triangle
    const vec3 v0 = vertices[i0 + geometryInfo.vertexOffset].position;
    const vec3 v1 = vertices[i1 + geometryInfo.vertexOffset].position;
    const vec3 v2 = vertices[i2 + geometryInfo.vertexOffset].position;
    // Get the barycentric coordinates of the intersection
    vec3 barycentrics = vec3(0.0, attributes.x, attributes.y);
    barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;
    // Get texcoord
    const vec2 u0 = vertices[i0 + geometryInfo.vertexOffset].texCoords;
    const vec2 u1 = vertices[i1 + geometryInfo.vertexOffset].texCoords;
    const vec2 u2 = vertices[i2 + geometryInfo.vertexOffset].texCoords;
    hit.uv = u0 * barycentrics.x + u1 * barycentrics.y + u2 * barycentrics.z;
#endif
    // Return hit geometry
    return hit;
}

#endif