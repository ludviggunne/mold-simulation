#version 450 core

#define TEXTURE_INT_BINDING 0
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
    
    vec4 acc = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = -2; i < 3; i++) {

        for (int j = -2; j < 3; j++) {

            ivec2 sample_point = coord + ivec2(i, j); 
            if (sample_point.x < 0 || sample_point.y < 0 || sample_point.x >= texture_width || sample_point.y >= texture_height) continue;

            acc += vec4(imageLoad(u_TextureInt, sample_point));
        }
    }

    vec4 color = 0.97 * acc / 25;

    imageStore(u_TextureInt, coord, color);
}