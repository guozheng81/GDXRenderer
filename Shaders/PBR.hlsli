#define PI 3.1415926f

float GGX(float3 N, float3 H, float roughness)
{
	float a = roughness*roughness;

	float a_sqr = a*a;
	float N_dot_H = max(dot(N, H), 0.0f);

	float bottom = N_dot_H*N_dot_H*(a_sqr - 1.0f) + 1.0f;
	return a_sqr / max(bottom*bottom*PI, 0.0001f);
}

float Geometry(float N_dot_V, float N_dot_L, float roughness)
{
	float k = (roughness + 1.0f)*(roughness + 1.0f) / 8.0f;
	//float k = roughness*roughness*0.5f;

	float G1 = N_dot_V / (N_dot_V*(1.0f - k) + k);
	float G2 = N_dot_L / (N_dot_L*(1.0f - k) + k);

	return G1*G2;
}

float3 Fresnel(float3 H, float3 V, float3 F_0)
{
	float H_dot_V = max(dot(H, V), 0.0f);
	//return F_0 + (1.0f -F_0)*pow((1.0f - H_dot_V), 5.0f);
	float F_C = pow((1.0f - H_dot_V), 5.0f);
	return saturate(50.0f*F_0.g)*F_C + (1.0f - F_C)*F_0;
}

float3 CalculatePBR(float3 L, float3 N, float3 V, float roughness, float metal, float3 albedo, float Radiance)
{
	float3 H = normalize(V + L);

	float N_dot_L = max(dot(N, L), 0.0f);
	float N_dot_V = max(abs(dot(N, V)), 0.0001f);

	float D = GGX(N, H, roughness);
	float G = Geometry(N_dot_V, N_dot_L, roughness);
	float3 SpecularColor = float3(0.04f, 0.04f, 0.04f);
	SpecularColor = lerp(SpecularColor, albedo.rgb, metal);
	float3 F = Fresnel(H, L, SpecularColor);

	float Kd = 1.0f - metal;

	float3 Specular = D*G*F / max((4.0f *N_dot_V*N_dot_L), 0.001f);
	return max((Kd*albedo / PI + Specular)*N_dot_L*Radiance, 0.0f);
}

float3 CalculateSpecular(float3 L, float3 N, float3 V, float roughness, float metal, float3 albedo)
{
	float3 H = normalize(V + L);

	float N_dot_L = max(dot(N, L), 0.0f);
	float N_dot_V = max(dot(N, V), 0.0f);

	float k = roughness*roughness / 2.0f;

	float G1 = 1.0f/*N_dot_V*/ / (N_dot_V*(1.0f - k) + k);
	float G2 = 1.0f/*N_dot_L*/ / (N_dot_L*(1.0f - k) + k);
	float G = G1*G2;
	float3 SpecularColor = float3(0.04f, 0.04f, 0.04f);
	SpecularColor = lerp(SpecularColor, albedo.rgb, metal);
	float3 F = Fresnel(H, L, SpecularColor);

	float3 Specular = G*F* max(dot(L, H), 0.0f)*N_dot_L / max(dot(N, H), 0.0001f) * albedo;

	return Specular;
}
