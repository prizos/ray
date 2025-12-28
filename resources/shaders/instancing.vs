#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

// Instancing: per-instance transformation matrix (passed as vertex attribute)
in mat4 instanceTransform;

// Input uniform values
uniform mat4 mvp;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;

void main()
{
    // Transform vertex position by instance transform
    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);
    fragPosition = worldPos.xyz;

    // Pass through texture coordinates
    fragTexCoord = vertexTexCoord;

    // Transform normal (simplified - no rotation handling for axis-aligned cubes)
    fragNormal = vertexNormal;

    // Final vertex position: MVP * instanceTransform * vertexPosition
    gl_Position = mvp * worldPos;
}
