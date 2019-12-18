/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

// nvidia denoise filter

#include "Common.hlsli"
#include "CommonHelper.hlsli"
#include "PBR.hlsli"
#include "Utils.hlsli"


StructuredBuffer<float4>	InColorBuffer : register(t0);
Texture2D<float4>		InNormalDepth : register(t1);
Texture2D<float4>		InPreNormalDepth : register(t2);

Texture2D<float4>		InIndirectColor : register(t4);

RWTexture2D<float4>		OutIndirectColor : register(u0);
RWStructuredBuffer<float4>		OutMoments : register(u1);
RWBuffer<uint>					OutHistoryCount : register(u2);


float4 GetPrePixelPosition(int2 PixelPos)
{
	float2 uv = (PixelPos + 0.5f) / ViewSizeAndInv.xy;

	float2 ScreenPos;
	ScreenPos.x = uv.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - uv.y*2.0f;

	float3 CameraRay = CameraXAxis.xyz*(ScreenPos.x) + CameraYAxis.xyz*(ScreenPos.y) + CameraZAxis.xyz;

	float4 WldPos = float4(CameraOrigin.xyz + CameraRay*InNormalDepth[PixelPos].w, 1.0f);

	float4	PreScreenPos = mul(WldPos, mPreViewProjection);
	PreScreenPos.xy /= PreScreenPos.w;
	PreScreenPos.x = (1.0f + PreScreenPos.x)*0.5f;
	PreScreenPos.y = (1.0f - PreScreenPos.y)*0.5f;

	return PreScreenPos;
}

float2	GetFwidthNormalDepth(int2 ipos)
{
	float4 Cur = InNormalDepth[ipos];
	int2 rightPos = ipos;
	rightPos.x = clamp(ipos.x + 1, 0, ViewSizeAndInv.x);
	float4 right = InNormalDepth[rightPos];

	int2 downPos = ipos;
	downPos.y = clamp(ipos.y + 1, 0, ViewSizeAndInv.y);
	float4 down = InNormalDepth[downPos];

	float4 dd_right = right - Cur;
	float4 dd_down = down - Cur;

	float2 res;
	res.x = length(abs(dd_right.xyz) + abs(dd_down.xyz));
	res.y = max(abs(dd_right.w), abs(dd_down.w));

	return res;
}

bool isReprjValid(int2 coord, float Z, float Zprev, float fwidthZ, float3 normal, float3 normalPrev, float fwidthNormal)
{

	const int2 imageDim = ViewSizeAndInv.xy;
	// check whether reprojected pixel is inside of the screen
	if (any((coord < int2(0, 0))) || any((coord > imageDim - int2(1, 1)))) return false;
	// check if deviation of depths is acceptable
	if (fwidthZ < 0.1f && abs(Zprev - Z) > 1.0f) return false;
	if (fwidthZ >= 0.1f && abs(Zprev - Z) / (fwidthZ + 1e-4) > 2.0) return false;

	// check normals for compatibility
	if (distance(normal, normalPrev) / (fwidthNormal + 1e-2) > 16.0) return false;

	return true;
}

bool loadPrevData(float2 fragCoord, out float4 prevIndirect, out float4 prevMoments, out float historyLength)
{
	const int2 ipos = floor(fragCoord);
	const float2 imageDim = ViewSizeAndInv.xy;

	// +0.5 to account for texel center offset
	float4 NormlizedPosPrev = GetPrePixelPosition(ipos);
	const int2 iposPrev = int2(NormlizedPosPrev.xy* imageDim);

	// stores: Z, fwidth(z), z_prev
	float4 normal_depth = InNormalDepth[ipos];
	float depth = NormlizedPosPrev.w;
	float3 normal = normal_depth.xyz;

//	prevIndirect = float4(abs(depth - normal_depth.w), 0.0f, 0.0f, 1.0f);
//	prevMoments = float4(0, 0, 0, 0);
//	historyLength = 1.0f;
//	return true;

	prevIndirect = float4(0, 0, 0, 0);
	prevMoments = float4(0, 0, 0, 0);

	float2 fwidth_normal_depth = GetFwidthNormalDepth(ipos);

	bool v[4];
	const float2 posPrev = NormlizedPosPrev.xy * imageDim;
	int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

	// check for all 4 taps of the bilinear filter for validity
	bool valid = false;
	for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
	{
		int2 loc = int2(posPrev)+offset[sampleIdx];
		float4 normal_depth_prev = InPreNormalDepth[loc];
		float depthPrev = normal_depth_prev.w;
		float3 normalPrev = normal_depth_prev.xyz;

		v[sampleIdx] = isReprjValid(iposPrev, depth, depthPrev, fwidth_normal_depth.y, normal, normalPrev, fwidth_normal_depth.x);

		valid = valid || v[sampleIdx];
	}

	if (valid)
	{
		float sumw = 0;
		float x = frac(posPrev.x);
		float y = frac(posPrev.y);

		// bilinear weights
		float w[4] = { (1 - x) * (1 - y),
			x  * (1 - y),
			(1 - x) *      y,
			x  *      y };

		prevIndirect = float4(0, 0, 0, 0);
		prevMoments = float4(0, 0, 0, 0);

		// perform the actual bilinear interpolation
		for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
		{
			int2 loc = int2(posPrev)+offset[sampleIdx];
			if (v[sampleIdx])
			{
				int	loc_idx = loc.y*ViewSizeAndInv.x + loc.x;
				prevIndirect += w[sampleIdx] * InIndirectColor[loc];
				prevMoments += w[sampleIdx] * OutMoments[loc_idx];
				sumw += w[sampleIdx];
			}
		}

		// redistribute weights in case not all taps were used
		valid = (sumw >= 0.01);
		prevIndirect = valid ? prevIndirect / sumw : float4(0, 0, 0, 0);
		prevMoments = valid ? prevMoments / sumw : float4(0, 0, 0, 0);
	}
	if (!valid) // perform cross-bilateral filter in the hope to find some suitable samples somewhere
	{
		float cnt = 0.0;

		// this code performs a binary descision for each tap of the cross-bilateral filter
		const int radius = 1;
		for (int yy = -radius; yy <= radius; yy++)
		{
			for (int xx = -radius; xx <= radius; xx++)
			{
				int2 p = iposPrev + int2(xx, yy);
				float4 normal_depth_filter = InPreNormalDepth[p];
				float depthFilter = normal_depth_filter.w;
				float3 normalFilter = normal_depth_filter.xyz;

				if (isReprjValid(iposPrev, depth, depthFilter, fwidth_normal_depth.y, normal, normalFilter, fwidth_normal_depth.x))
				{
					int p_idx = p.y*ViewSizeAndInv.x + p.x;
					prevIndirect += InIndirectColor[p];
					prevMoments += OutMoments[p_idx];
					cnt += 1.0;
				}
			}
		}
		if (cnt > 0)
		{
			valid = true;
			prevIndirect /= cnt;
			prevMoments /= cnt;
		}

	}

	if (valid)
	{
		// crude, fixme
		historyLength = OutHistoryCount[iposPrev.y*ViewSizeAndInv.x + iposPrev.x];
	}
	else
	{
		prevIndirect = float4(0, 0, 0, 0);
		prevMoments = float4(0, 0, 0, 0);
		historyLength = 0;
	}

	return valid;
}

float luminance(float3 InColor)
{
	return dot(InColor, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void SVGFReprojectCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = GTid.x * 16 + GTid.y;

	uint PixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;

	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 fragCoord = (DTid.xy + 0.5f);

	const int2 ipos = DTid.xy;
	float3 indirect;
	indirect = InColorBuffer[ipos.y*ViewSizeAndInv.x + ipos.x].rgb;

	float historyLength;
	float4 prevIndirect, prevMoments;
	bool success = loadPrevData(fragCoord.xy, prevIndirect, prevMoments, historyLength);
	historyLength = min(16.0f, success ? historyLength + 1.0f : 1.0f);


	float gAlpha = 0.2f;
	float gMomentsAlpha = 0.1f;

	// this adjusts the alpha for the case where insufficient history is available.
	// It boosts the temporal accumulation to give the samples equal weights in
	// the beginning.
	const float alpha = success ? max(gAlpha, 1.0 / historyLength) : 1.0;
	const float alphaMoments = success ? max(gMomentsAlpha, 1.0 / historyLength) : 1.0;

	// compute first two moments of luminance
	float4 moments = float4(0.0f, 0.0f, 0.0f, 0.0f);
	moments.b = luminance(indirect);
	moments.a = moments.b * moments.b;

	// temporal integration of the moments
	moments = lerp(prevMoments, moments, alphaMoments);

	GroupMemoryBarrierWithGroupSync();

	OutMoments[PixelIdx] = moments;
	OutHistoryCount[PixelIdx] = historyLength;

	float2 variance = max(float2(0, 0), moments.ga - moments.rb * moments.rb);

	//variance *= computeVarianceScale(16, 16, alpha);

	// temporal integration of direct and indirect illumination
	// variance is propagated through the alpha channel
	float4 FinalColor = lerp(prevIndirect, float4(indirect, 0), alpha);
	FinalColor.a = variance.g;
	OutIndirectColor[DTid.xy] = FinalColor;
}

float normalDistanceCos(float3 n1, float3 n2, float power)
{
	//return pow(max(0.0, dot(n1, n2)), 128.0);
	//return pow( saturate(dot(n1,n2)), power);
	return 1.0f;
}

float normalDistanceTan(float3 a, float3 b)
{
	const float d = max(1e-8, dot(a, b));
	return sqrt(max(0.0, 1.0 - d * d)) / d;
}

float2 computeWeight(
	float depthCenter, float depthP, float phiDepth,
	float3 normalCenter, float3 normalP, float normPower,
	float luminanceIndirectCenter, float luminanceIndirectP, float phiIndirect)
{
	const float wNormal = normalDistanceCos(normalCenter, normalP, normPower);
	const float wZ = (phiDepth == 0) ? 0.0f : abs(depthCenter - depthP) / phiDepth;
	const float wLindirect = abs(luminanceIndirectCenter - luminanceIndirectP) / phiIndirect;

	const float wIndirect = exp(0.0 - max(wLindirect, 0.0) - max(wZ, 0.0)) * wNormal;

	return float2(0.0f, wIndirect);
}

float computeWeightNoLuminance(float depthCenter, float depthP, float phiDepth, float3 normalCenter, float3 normalP)
{
	const float wNormal = normalDistanceCos(normalCenter, normalP, 128.0f);
	const float wZ = abs(depthCenter - depthP) / phiDepth;

	return exp(-max(wZ, 0.0)) * wNormal;
}

#define gPhiColor 10.0f
#define gPhiNormal 128.0f

Buffer<int>	InHistoryCount : register(t0);
StructuredBuffer<float4>		InMoments : register(t3);

[numthreads(16, 16, 1)]
void SVGFMomentFilterCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint pixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;

	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 fragCoord = (DTid.xy + 0.5f);

	const int2 ipos = DTid.xy;

	float h = InHistoryCount[ipos.y*ViewSizeAndInv.x + ipos.x].r;
	int2 screenSize = ViewSizeAndInv.xy;

	float2 fwidth_normal_depth = GetFwidthNormalDepth(ipos);

	float3  sumIndirect = float3(0.0, 0.0, 0.0);
	float4  sumMoments = float4(0.0, 0.0, 0.0, 0.0);
	float2 variance = float2(0.0f, 0.0f);

	if (h < 4.0) // not enough temporal history available
	{
		float sumWIndirect = 0.0;
		sumIndirect = float3(0.0, 0.0, 0.0);
		sumMoments = float4(0.0, 0.0, 0.0, 0.0);

		const float4  indirectCenter = InIndirectColor[DTid.xy];
		const float lIndirectCenter = luminance(indirectCenter.rgb);

		float3 normalCenter = InNormalDepth[ipos].xyz;
		float zCenter = InNormalDepth[ipos].w;

		const float phiLIndirect = gPhiColor;
		const float phiDepth = max(fwidth_normal_depth.y, 1e-10)* 4.0f;

		// compute first and second moment spatially. This code also applies cross-bilateral
		// filtering on the input color samples
		const int radius = 3;

		for (int yy = -radius; yy <= radius; yy++)
		{
			for (int xx = -radius; xx <= radius; xx++)
			{
				const int2 p = ipos + int2(xx, yy);
				const bool inside = all((p>= int2(0, 0))) && all((p < screenSize));
				const float kernel = 1.0;

				if (inside)
				{
					int p_idx = p.y*ViewSizeAndInv.x + p.x;

					const float3 indirectP = InIndirectColor[p].rgb;
					const float4 momentsP = InMoments[p_idx];

					const float lIndirectP = luminance(indirectP.rgb);

					float3 normalP = InNormalDepth[p].xyz;
					float zP = InNormalDepth[p].w;

					const float2 w = computeWeight(
						zCenter, zP, phiDepth * length(float2(xx, yy)),
						normalCenter, normalP, gPhiNormal,
						lIndirectCenter, lIndirectP, phiLIndirect);

					const float wIndirect = w.y;

					sumWIndirect += wIndirect;
					sumIndirect += indirectP * wIndirect;

					sumMoments += momentsP * float4(0.0f, 0.0f, wIndirect.xx);
				}
			}
		}

		// Clamp sums to >0 to avoid NaNs.
		sumWIndirect = max(sumWIndirect, 1e-6f);

		sumIndirect /= sumWIndirect;
		sumMoments /= float4(1e-6f, 1e-6f, sumWIndirect.xx);

		// compute variance for direct and indirect illumination using first and second moments
		variance = sumMoments.ga - sumMoments.rb * sumMoments.rb;

		// give the variance a boost for the first frames
		variance *= 2.0 / h;
	}

	if (h < 4.0)
	{
		OutIndirectColor[DTid.xy] = float4(sumIndirect, variance.g);
	}
	else
	{
		OutIndirectColor[DTid.xy] = InIndirectColor[DTid.xy];
	}

}

//groupshared float4	IndirectColorShared[20][20];

// computes a 3x3 gaussian blur of the variance, centered around
// the current pixel
float2 computeVarianceCenter(int2 ipos, Texture2D<float4> sIndirect)
{
	float2 sum = float2(0.0, 0.0);

	const float kernel[2][2] = {
		{ 1.0 / 4.0, 1.0 / 8.0 },
		{ 1.0 / 8.0, 1.0 / 16.0 }
	};

	const int radius = 1;
	for (int yy = -radius; yy <= radius; yy++)
	{
		for (int xx = -radius; xx <= radius; xx++)
		{

			float k = kernel[abs(xx)][abs(yy)];

//#if ATROUS_STEP_SIZE == 1
//			int2 p = ipos + int2(xx, yy);
//			int2 p1 = p + int2(2, 2);
//			sum.g += IndirectColorShared[p1.y][p1.x].a * k;
//#else
			int2 p = ipos + int2(xx, yy);
			p = clamp(p, int2(0, 0), int2(ViewSizeAndInv.xy) - 1);
			int p_idx = p.y*ViewSizeAndInv.x + p.x;

			sum.g += sIndirect[p].a * k;
//#endif
		}
	}

	return sum;
}

static const float	JitterSamples[4][4] = { { 0.0f, 0.5f, 0.125f, 0.625f } ,
{ 0.75f, 0.25f, 0.875f, 0.375f },
{ 0.1875f, 0.6875f, 0.0625f, 0.5625 } ,
{ 0.9375f, 0.4375f, 0.8125f, 0.3125 } };

float4 SampleIndirectColor(int2 ipos)
{
	float RandRotation = frac(sin(dot((ipos + 0.5f)/ViewSizeAndInv.xy, float2(12.9898f, 78.233f))) * (43758.5453f));
	float theta = RandRotation * 3.1415926f*2.0f;
	float2x2 randomRotationMatrix = float2x2(float2(cos(theta), -sin(theta)),
		float2(sin(theta), cos(theta)));

	float	JitterValue = JitterSamples[ipos.x % 4][ipos.y % 4];
	float2 randomOffset = mul(float2(JitterValue, 0.0f), randomRotationMatrix);
	float2 pos = ipos + randomOffset + float2(0.5, 0.5);

	float x = frac(pos.x);
	float y = frac(pos.y);

	float w[4] = { (1 - x) * (1 - y),
		x  * (1 - y),
		(1 - x) *      y,
		x  *      y };

	float4 FinalColor = float4(0, 0, 0, 0);

	int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

	// perform the actual bilinear interpolation
	for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
	{
		int2 loc = int2(pos)+offset[sampleIdx];
		loc = clamp(loc, int2(0, 0), int2(ViewSizeAndInv.xy) - 1);
		int	loc_idx = loc.y*ViewSizeAndInv.x + loc.x;
		FinalColor += w[sampleIdx] * InIndirectColor[loc];
	}

	return FinalColor;
}

RWTexture2D<float4>		OutAtrousColor : register(u1);

[numthreads(16, 16, 1)]
void SVGFAtrousCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint pixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;


	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 fragCoord = (DTid.xy + 0.5f);

	const int2 ipos = DTid.xy;

	float historyLength = InHistoryCount[ipos.y*ViewSizeAndInv.x + ipos.x].r;
	int2 screenSize = ViewSizeAndInv.xy;

	const float epsVariance = 1e-10;
	const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

	// constant samplers to prevent the compiler from generating code which
	// fetches the sampler descriptor from memory for each texture access
//#if ATROUS_STEP_SIZE == 1
//	const float4  indirectCenter = IndirectColorShared[GTid.y+2][GTid.x+2];
//#else
	float4  indirectCenter = InIndirectColor[DTid.xy];
	//float4  indirectCenter = SampleIndirectColor(ipos);

//#endif
	const float lIndirectCenter = luminance(indirectCenter.rgb);

	// variance for direct and indirect, filtered using 3x3 gaussin blur
	const float2 var = computeVarianceCenter(ipos, InIndirectColor);

	float2 fwidth_normal_depth = GetFwidthNormalDepth(ipos);

	float3 normalCenter = InNormalDepth[ipos].xyz;
	float zCenter = InNormalDepth[ipos].w;


	const float phiLIndirect = gPhiColor * sqrt(max(0.0, epsVariance + var.g));
	const float phiDepth = max(fwidth_normal_depth.y, 1e-10) * ATROUS_STEP_SIZE;

	// explicitly store/accumulate center pixel with weight 1 to prevent issues
	// with the edge-stopping functions
	float sumWIndirect = 1.0;
	float sumWIndirectSquared = 1.0f;
	float4  sumIndirect = indirectCenter;

	for (int yy = -2; yy <= 2; yy++)
	{
		for (int xx = -2; xx <= 2; xx++)
		{
			const int2 p = ipos + int2(xx, yy) * ATROUS_STEP_SIZE;
			const bool inside = all((p >= int2(0, 0))) && all((p < screenSize));

			const float kernel = kernelWeights[abs(xx)] * kernelWeights[abs(yy)];

			if (inside && (xx != 0 || yy != 0)) // skip center pixel, it is already accumulated
			{
				int p_idx = p.y*ViewSizeAndInv.x + p.x;

				const float4 indirectP = InIndirectColor[p];
				//float4  indirectP = SampleIndirectColor(p);

				float3 normalP = InNormalDepth[p].xyz;
				float zP = InNormalDepth[p].w;

				const float lIndirectP = luminance(indirectP.rgb);

				// compute the edge-stopping functions
				const float2 w = computeWeight(
					zCenter.x, zP.x, phiDepth * length(float2(xx, yy)),
					normalCenter, normalP, gPhiNormal,
					lIndirectCenter, lIndirectP, phiLIndirect);

				const float wIndirect = w.y * kernel;

				// alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula

				sumWIndirect += wIndirect;
				sumWIndirectSquared += wIndirect*wIndirect;
				sumIndirect += float4(wIndirect.xxxx) * indirectP;
			}
		}
	}

	// renormalization is different for variance, check paper for the formula
	float4 FinalColor = float4(sumIndirect / sumWIndirect.xxxx);
	OutIndirectColor[DTid.xy] = FinalColor;

#if ATROUS_STEP_SIZE == 1
	OutAtrousColor[DTid.xy] = FinalColor;
#endif

}