// Update On Render Dragon by Naresh rain ripple

#ifndef RAIN_H
#define RAIN_H

#include "clouds.h"
#include "detection.h"
#include "noise.h"
#include "sky.h"
#include "water.h"

float nlWindblow(vec3 pos, float t){
  vec2 p = pos.xy/(1.0+pos.z);
  float val = sin(4.0*p.x + 2.0*p.y + 2.0*t + 3.0*p.y*p.x)*sin(p.y*2.0 + 0.2*t);
  val += sin(p.y - p.x + 0.2*t);
  return 0.25*val*val;
}

// Rain drop ripples (ported from TSPE GLSL shader) 
#ifdef NL_RAIN_RIPPLE

// scalar hash (was hash_rain) - random start time of each drop
float nlRippleHash1(highp vec2 p) {
  highp vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yxz + 19.19);
  return fract((p3.x + p3.y) * p3.z);
}

// vector hash (was hash_ripple) - random drop position inside a cell
vec2 nlRippleHash2(highp vec2 p) {
  highp vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1031, 0.0973));
  p3 += dot(p3, p3.yzx + 19.19);
  return fract((p3.xx + p3.yz) * p3.zy);
}

// returns surface normal of the ripple field at world xz position
vec3 nlRainRipple(highp vec2 worldXZ, highp float t) {
  highp vec2 p3 = worldXZ * NL_RAIN_RIPPLE_SCALE;
  highp vec2 p0 = floor(p3);
  vec2 circles = vec2(0.0, 0.0);

  for (int j = -NL_RAIN_RIPPLE_RADIUS; j <= NL_RAIN_RIPPLE_RADIUS; j++) {
    for (int i = -NL_RAIN_RIPPLE_RADIUS; i <= NL_RAIN_RIPPLE_RADIUS; i++) {
      highp vec2 cell = p0 + vec2(float(i), float(j));
      highp vec2 p = cell + nlRippleHash2(cell);
      float tt = fract(NL_RAIN_RIPPLE_SPEED*t + nlRippleHash1(cell));
      vec2 v = p - p3;
      float len = max(length(v), 0.0001);
      float d = len - (float(NL_RAIN_RIPPLE_RADIUS) + 1.0) * tt;
      float h = 0.001;
      float d1 = d - h;
      float d2 = d + h;
      float p1 = sin(31.0*d1) * smoothstep(-0.6, -0.3, d1) * smoothstep(0.0, -0.3, d1);
      float p2 = sin(31.0*d2) * smoothstep(-0.6, -0.3, d2) * smoothstep(0.0, -0.3, d2);
      float fade = 1.0 - tt;
      circles += 0.5 * (v/len) * ((p2 - p1)/(2.0*h)) * fade * fade;
    }
  }

  float cells = float((NL_RAIN_RIPPLE_RADIUS*2 + 1) * (NL_RAIN_RIPPLE_RADIUS*2 + 1));
  circles /= cells;

  // keep sqrt() valid
  float cl = length(circles);
  circles *= min(1.0, 0.95/max(cl, 0.0001));

  return vec3(circles, sqrt(1.0 - dot(circles, circles)));
}

// rippleStrength: v_position.w (rain * sky exposure), brightness: day/torch light
// returns vec4(rgb highlight, darkening)
vec4 nlRainRippleShade(highp vec2 worldXZ, highp float t, float rippleStrength, float fogA, float brightness) {
  vec3 n = nlRainRipple(worldXZ, t);

  // patchy wet areas
  float patchy = clamp(pow(noise2D((worldXZ - 0.5)*0.5)*1.5, 2.0), 0.0, 1.0);
  patchy = mix(1.0, patchy, NL_RAIN_RIPPLE_PATCHY);

  // fade with distance (fog)
  float distFade = pow(clamp(1.0 - fogA*2.0, 0.0, 1.0), 2.0);

  float k = rippleStrength * patchy * distFade;
  float spec = pow(clamp(dot(n, normalize(vec3(1.0, 0.7, 0.5))), 0.0, 1.0), 6.0);

  float highlight = 5.0 * spec * brightness * k;
  float darken = clamp(length(n.xy)*0.6, 0.0, 0.4) * k;
  return vec4(highlight, highlight, highlight, darken);
}

#endif

vec4 nlRefl(
  inout vec4 color, nl_skycolor skycol, nl_environment env, vec3 viewDir, vec3 wPos, vec3 tiledCpos,
  vec3 CAMERA_POS, vec3 torchColor, vec2 lit, float camDist, float renderDist, highp float t
) {
  vec4 wetRefl = vec4(0.0,0.0,0.0,0.0);

  #ifndef NL_GROUND_REFL
  if (env.rainFactor > 0.0) {
  #endif

    float wetness = lit.y*lit.y;

    // clip reflection when far (better performance)
    float endDist = renderDist*0.6;
    if (camDist < endDist) {
      float cosR = max(viewDir.y, 0.0);
      float puddles = max(1.0 - NL_GROUND_RAIN_PUDDLES*fastRand(tiledCpos.xz), 0.0);

      #ifndef NL_GROUND_REFL
        wetness *= puddles;
        float reflective = wetness*env.rainFactor*NL_GROUND_RAIN_WETNESS;
      #else
        float reflective = NL_GROUND_REFL;
        if (!env.end && !env.nether) {
          // only multiply with wetness in overworld
          reflective *= wetness;
        } 

        wetness *= puddles;
        reflective = mix(reflective, wetness, env.rainFactor);
      #endif

      if (wPos.y < 0.0) {
        viewDir.y = -viewDir.y;
        wetRefl.rgb = nlRenderSky(skycol, env, viewDir, t, false);

        #ifdef NL_CLOUD_AURORA_REFLECTION
          vec4 cloudRefl = nlCloudAuroraReflection(skycol, env, viewDir, wPos, CAMERA_POS, t);
          wetRefl.rgb = mix(wetRefl.rgb, cloudRefl.rgb, cloudRefl.a);
        #endif

        // torch light
        wetRefl.rgb += torchColor*lit.x*NL_TORCHLIGHT_INTENSITY;

        wetRefl.a = calculateFresnel(cosR, 0.03)*reflective;
        wetRefl.a *= clamp(2.0-2.0*camDist/endDist, 0.0, 1.0); // fade out before clip
      }
    }

    // darken wet parts
    color.rgb *= 1.0 - 0.4*wetness*env.rainFactor;

  #ifndef NL_GROUND_REFL
  }
  #endif

  return wetRefl;
}

#endif
