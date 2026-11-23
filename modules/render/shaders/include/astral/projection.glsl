#ifndef ASTRAL_PROJECTION_GLSL
#define ASTRAL_PROJECTION_GLSL

struct TextureCoords {
    vec2 uv;
    vec3 p;
    vec3 weights;
    vec3 sign_axis;
    mat3 axes;
};

TextureCoords material_coords(Material m, mat4 model, vec3 world, vec3 n, vec2 uv) {
    TextureCoords c;
    float s = sin(m.projection_phase.w), r = cos(m.projection_phase.w);
    c.uv = mat2(r, s, -s, r) * (uv * m.uv_transform.xy) + m.uv_transform.zw;
    c.axes = mat3(1.0);
    c.p = world;
    c.weights = vec3(0.0);
    c.sign_axis = vec3(1.0);
    if (m.projection.x < 0.5) return c;

    if (m.projection.w < 0.5) {
        c.axes = mat3(normalize(model[0].xyz), normalize(model[1].xyz), normalize(model[2].xyz));
        c.p = transpose(c.axes) * (world - model[3].xyz) * m.projection.y;
    } else {
        c.p = world * m.projection.y + m.projection_phase.xyz;
    }
    vec3 local_n = transpose(c.axes) * n;
    c.sign_axis = mix(vec3(-1.0), vec3(1.0), greaterThanEqual(local_n, vec3(0.0)));
    if (m.projection.x == 1.0) c.weights.z = 1.0;
    else if (m.projection.x == 2.0) c.weights.y = 1.0;
    else if (m.projection.x == 3.0) c.weights.x = 1.0;
    else {
        c.weights = pow(abs(local_n), vec3(m.projection.z));
        c.weights /= max(dot(c.weights, vec3(1.0)), 1e-8);
    }
    return c;
}

vec2 projection_uv(TextureCoords c, int axis, vec2 offset) {
    int u = (axis + 1) % 3, v = (axis + 2) % 3;
    return vec2(c.p[u] * c.sign_axis[axis], c.p[v]) + offset;
}

vec4 material_sample(uint tex, Material m, TextureCoords c, float lod) {
    if (m.projection.x < 0.5) return ASTRAL_PROJECT_SAMPLE(tex, c.uv, lod);
    vec4 result = vec4(0.0);
    for (int axis = 0; axis < 3; ++axis) {
        if (c.weights[axis] > 0.0) {
            result += c.weights[axis] * ASTRAL_PROJECT_SAMPLE(tex, projection_uv(c, axis, m.uv_transform.zw), lod);
        }
    }
    return result;
}

vec3 projected_normal(uint tex, Material m, TextureCoords c, vec3 n, float lod) {
    vec3 result = vec3(0.0);
    for (int axis = 0; axis < 3; ++axis) {
        if (c.weights[axis] <= 0.0) continue;
        vec2 xy = ASTRAL_PROJECT_SAMPLE(tex, projection_uv(c, axis, m.uv_transform.zw), lod).xy * 2.0 - 1.0;
        float z = sqrt(max(0.0, 1.0 - dot(xy, xy)));
        vec3 t = c.axes[(axis + 1) % 3] * c.sign_axis[axis];
        vec3 b = c.axes[(axis + 2) % 3];
        vec3 perturb = (t * xy.x + b * xy.y) * m.params.y;
        result += c.weights[axis] * (n * z + perturb - n * dot(n, perturb));
    }
    return dot(result, result) > 1e-12 ? normalize(result) : n;
}
#endif
