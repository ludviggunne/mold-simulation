#version 450 core

#define TEXTURE_INT_BINDING    0
#define UNIFORM_BUFFER_BINDING 1

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (rgba32f, binding = TEXTURE_INT_BINDING) uniform image2D u_TextureInt;

layout (std140, binding = UNIFORM_BUFFER_BINDING) uniform UniformBuffer {

    uint   texture_width;
    uint   texture_height;
    uint   patch_size;
    uvec2  patch_offset;
    uint   agent_count;
    float  delta_time;
};

void main() {

    ivec2 coord = ivec2(patch_offset + gl_GlobalInvocationID.xy);
    if (coord.x >= texture_width || coord.y >= texture_height) return;

    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

    imageStore(u_TextureOut, coord, color);
}