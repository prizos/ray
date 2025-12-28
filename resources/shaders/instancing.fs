#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Simple shading: use diffuse color with basic directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(fragNormal);

    // Ambient + diffuse lighting
    float ambient = 0.4;
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.6;
    float lighting = ambient + diffuse;

    // Apply lighting to diffuse color
    finalColor = vec4(colDiffuse.rgb * lighting, colDiffuse.a);
}
