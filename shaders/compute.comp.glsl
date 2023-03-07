#version 450 core

#define TEXTURE_INT_BINDING 0
#define UNIFORM_BUFFER_BINDING 1
#define SHADER_STORAGE_BUFFER_BINDING 2

layout (local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout (rgba32f, binding = TEXTURE_INT_BINDING) uniform image2D u_TextureInt;

layout (std140, binding = UNIFORM_BUFFER_BINDING) uniform UniformBuffer {

    uint   texture_width;
    uint   texture_height;
    uint   patch_size;
    uvec2  patch_offset;
    uint   agent_count;
    float  delta_time;
};

uniform float uSampleDistL;
uniform float uSampleDistR;
uniform float uSampleAngleL;
uniform float uSampleAngleR;
uniform float uAgentSpeed;
uniform float uTurnSpeed;

struct agent_t {

    vec2  position;
    float angle;
    vec3  signature;
};

layout (std140, binding = SHADER_STORAGE_BUFFER_BINDING) buffer ShaderStorageBuffer {

    agent_t agents[];
};

void main() {

    uint index = patch_offset.x + gl_GlobalInvocationID.x;
    agent_t agent = agents[index];

    ivec2 coord = ivec2(agent.position);
    agent.position += delta_time * 200.0 * vec2(cos(agent.angle), sin(agent.angle));

    if (agent.position.x < 0.0 || agent.position.x >= texture_width)  agent.angle =  -1.0 * (agent.angle + 3.1415 / 2) - 3.1415 / 2;
    if (agent.position.y < 0.0 || agent.position.y >= texture_height) agent.angle *= -1.0;

    vec2 sample_point1 = agent.position + 20.0 * vec2(cos(agent.angle + 3.1415 / 4), sin(agent.angle + 3.1415 / 4));
    vec2 sample_point2 = agent.position + 40.0 * vec2(cos(agent.angle - 3.1415 / 2), sin(agent.angle - 3.1415 / 2));

    vec3 sample1 = agent.signature * imageLoad(u_TextureInt, ivec2(sample_point1)).xyz;
    vec3 sample2 = agent.signature * imageLoad(u_TextureInt, ivec2(sample_point2)).xyz;

    float diff = dot(sample1 - sample2, vec3(1.0)) / 3;

    agent.angle += delta_time * 80.0 * diff;

    imageStore(u_TextureInt, coord, vec4(agent.signature, 1.0));
    agents[index] = agent;
}