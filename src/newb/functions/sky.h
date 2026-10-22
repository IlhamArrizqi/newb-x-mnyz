#ifndef SKY_H
#define SKY_H

#include "detection.h"
#include "noise.h"

struct nl_skycolor {
  vec3 zenith;
  vec3 horizon;
  vec3 horizonEdge;
};

// rainbow spectrum
vec3 spectrum(float x) {
  vec3 s = vec3(x-0.5, x, x+0.5);
  s = smoothstep(1.0, 0.0, abs(s));
  return s*s;
}

vec3 getUnderwaterCol(vec3 FOG_COLOR) {
  return 2.0 * NL_UNDERWATER_TINT * FOG_COLOR * FOG_COLOR;
}

vec3 getEndZenithCol() { return NL_END_ZENITH_COL; }
vec3 getEndHorizonCol() { return NL_END_HORIZON_COL; }

nl_skycolor nlEndSkyColors(nl_environment env) {
  nl_skycolor s;
  s.zenith = getEndZenithCol();
  s.horizon = getEndHorizonCol();
  s.horizonEdge = s.horizon;
  return s;
}

nl_skycolor nlOverworldSkyColors(nl_environment env) {
  nl_skycolor s;
  float f = 1.0 + 2.0*(1.0-max(-env.dayFactor, 0.0));
  float nightFactor = step(env.dayFactor, 0.0);
  s.zenith = mix(NL_DAY_ZENITH_COL, NL_NIGHT_ZENITH_COL*f, nightFactor);
  s.horizon = mix(NL_DAY_HORIZON_COL, NL_NIGHT_HORIZON_COL*f, nightFactor);
  s.horizonEdge = mix(NL_DAY_EDGE_COL, NL_NIGHT_EDGE_COL*f, nightFactor);

  float dawnFactor = 1.0 - smoothstep(0.0, 0.3, abs(env.dayFactor));
  dawnFactor *= dawnFactor*dawnFactor;
  dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
  s.zenith = mix(s.zenith, NL_DAWN_ZENITH_COL, dawnFactor);
  s.horizon = mix(s.horizon, NL_DAWN_HORIZON_COL, dawnFactor);
  s.horizonEdge = mix(s.horizonEdge, NL_DAWN_EDGE_COL, dawnFactor);

  float zh = dot(s.zenith, vec3_splat(0.33));
  float hh = dot(s.horizon, vec3_splat(0.33));
  float rainMix = env.rainFactor*NL_SKY_RAIN_MIX_FACTOR;
  s.zenith = mix(s.zenith, NL_RAIN_ZENITH_COL*zh, rainMix);
  s.horizon = mix(s.horizon, NL_RAIN_HORIZON_COL*hh, rainMix);
  s.horizonEdge = mix(s.horizonEdge, s.horizon, env.rainFactor);

  if (env.underwater) {
    vec3 underwaterFog = env.fogCol*env.fogCol*NL_UNDERWATER_TINT;
    s.zenith = mix(2.0*underwaterFog, underwaterFog*zh, 0.8);
    s.horizon = mix(2.0*underwaterFog, underwaterFog*hh, 0.8);
    s.horizonEdge = s.horizon;
  }

  return s;
}

nl_skycolor nlSkyColors(nl_environment env) {
  if (env.end) {
    return nlEndSkyColors(env);
  }
  return nlOverworldSkyColors(env);
}


vec3 renderOverworldSky(nl_skycolor skyCol, nl_environment env, vec3 viewDir, bool isSkyPlane) {
  float avy = abs(viewDir.y);
  float mask = 0.5 + (0.5*viewDir.y/(0.4 + avy));

  vec2 g = clamp(0.5 - 0.5*vec2(dot(env.sunDir, viewDir), dot(env.moonDir, viewDir)), 0.0, 1.0);
  vec2 g1 = 1.0-mix(sqrt(g), g, env.rainFactor);
  vec2 g2 = g1*g1;
  vec2 g4 = g2*g2;
  vec2 g8 = g4*g4;
  float mg8 = (g8.x+g8.y)*mask*(1.0-0.9*env.rainFactor);

  float vh = 1.0 - viewDir.y*viewDir.y;
  float vh2 = vh*vh;
  vh2 = mix(vh2, mix(1.0, vh2*vh2, NL_SKY_VOID_FACTOR), step(viewDir.y, 0.0));
  vh2 = mix(vh2, 1.0, mg8);
  float vh4 = vh2*vh2;

  float gradient1 = vh4*vh4;
  float zenithPull = 3.5; 
  float gradient2 = 0.8*gradient1 + 0.2*pow(vh2, zenithPull);

  gradient1 *= gradient1;
  gradient1 = mix(gradient1*gradient1, 1.0, mg8);
  gradient2 = mix(gradient2, 1.0, mg8);

  float dawnFactor = 1.0 - smoothstep(0.0, 0.3, abs(env.dayFactor));
  float df = 1.0;
  vec3 sky = mix(skyCol.horizon, skyCol.horizonEdge, gradient1*df*df);
  sky = mix(skyCol.zenith, sky, gradient2*df);
  
  float dawnGlow = g2.x * vh4 * dawnFactor * dawnFactor;
  sky += vec3(2.9, 0.9, 0.1) * dawnGlow * (1.0 - env.rainFactor);

  sky *= 0.5+0.5*gradient2;
  sky *= (1.0 + (2.0*mg8 + 7.0*mg8*mg8)*mask)*mix(1.0, mask, NL_SKY_VOID_DARKNESS);

  if (!isSkyPlane) {
    float source = max(0.0, (mg8-0.22)/0.78);
    source *= source;
    source *= source;
    sky *= 1.0 + 15.0*source*(1.0-env.rainFactor);
  }

  #ifdef NL_RAINBOW
    float rainbowFade = 0.5 + 0.5 * viewDir.y;
    rainbowFade *= rainbowFade;
    rainbowFade *= mix(NL_RAINBOW_CLEAR, NL_RAINBOW_RAIN, env.rainFactor);
    rainbowFade *= 0.5 + 0.5 * env.dayFactor;
    sky += spectrum(24.2 * (0.85 - g.x)) * rainbowFade * skyCol.horizon;
  #endif

  return sky;
}

vec3 renderEndSky(vec3 horizonCol, vec3 zenithCol, vec3 viewDir, float t) {
    float a = atan2(viewDir.x, viewDir.z);
    float y = viewDir.y;
    float yb = y - BAND_ELEVATION;
    float tt = t * 0.22;

    a += tt * PILLAR_ROTATION_SPEED;
    float zenithSafe = 1.0 - smoothstep(ZENITH_SAFE_START, ZENITH_SAFE_END, y);

    float s5  = sin(a * 5.0);
    float s8  = sin(a * 8.0);
    float s13 = sin(a * 13.0);

    float w1n = s5 * 0.5 + 0.5;
    float w2n = s8 * 0.5 + 0.5;
    float w3n = s13 * 0.5 + 0.5;

    float b1 = w1n * w1n;
    float b2 = w2n * w2n * w2n;
    float b3 = w3n * w3n;
    b3 = b3 * b3;

    float bars = b1 * 0.25 + b2 * 0.55 + b3 * 0.20;

    float h1 = sin(a * 3.0 + tt * 2.2);
    float maxHeight = 0.42 + 0.55 * h1 + 0.20 * s5;
    maxHeight = clamp(maxHeight, 0.25, 0.95);

    float vert = max(0.0, 1.0 - abs(yb) / maxHeight);
    vert = vert * vert;
    vert *= 1.0 - abs(yb) * 0.35;

    float beams = bars * vert;

    float glowBars = b1 * 0.3 + b2 * 0.7;
    float glowVert = max(0.0, 1.0 - abs(yb) / (maxHeight * 2.4));
    glowVert = glowVert * glowVert;
    float glow = glowBars * glowVert * 0.8;

    float bandPulse = 0.5 + 0.6 * sin(tt * 1.8);
    float bandHeight = 0.12 + 0.06 * bandPulse;
    float band = exp(-abs(yb) / bandHeight);

    float localBoost = bars * exp(-abs(yb) / 0.28) * 0.5;
    band += localBoost;

    float bandColT = 0.95 + 0.5 * sin(tt * 1.0);
    vec3 bandCol = mix(NL_STREAK_COL_PRIMARY * 0.7, NL_STREAK_COL_SECONDARY * 0.9, bandColT);

    float grad = 0.5 + 0.5 * y;
    float grad2 = grad * grad;
    float horizonFade = smoothstep(1.0, -0.6, y);
    
    float f = 0.25 * beams + 0.75 * horizonFade;
    vec3 sky = mix(zenithCol, horizonCol, f * f * f);

    float bottomFade = 1.0 - 0.7 * smoothstep(-0.4, -1.0, y);
    vec3 glowCol = mix(NL_STREAK_COL_PRIMARY, NL_STREAK_COL_SECONDARY, b2);
    vec3 coreCol = NL_STREAK_COL_ZENITH * (0.6 + 0.4 * b3);

    sky += 0.005 * glow * bottomFade * glowCol * 1.7;
    sky += 0.95 * beams * bottomFade * glowCol * zenithSafe;
    sky += 0.55 * b2 * vert * bottomFade * NL_STREAK_COL_SECONDARY * zenithSafe;
    sky += 1.3 * b3 * vert * coreCol * grad2 * zenithSafe;
    sky += band * bandCol * 2.5 * bottomFade * zenithSafe;

    return sky;
}

vec3 nlRenderSky(nl_skycolor skycol, nl_environment env, vec3 viewDir, float t, bool isSkyPlane) {
  vec3 sky;
  viewDir.y = -viewDir.y;

  if (env.end) {
    if (isSkyPlane) {
      sky = renderEndSky(skycol.horizon, skycol.zenith, viewDir, t);
    } else {
      float y = viewDir.y;
      float horizonFade = smoothstep(1.0, -0.6, y);
      sky = mix(skycol.zenith, skycol.horizon, horizonFade * horizonFade * horizonFade);
    }
  } else {
    sky = renderOverworldSky(skycol, env, viewDir, isSkyPlane);
    #ifdef NL_UNDERWATER_STREAKS
      if (env.underwater) {
        float a = atan2(viewDir.x, viewDir.z);
        float grad = 0.5 + 0.5 * viewDir.y;
        grad *= grad;
        float spread = (0.5 + 0.5 * sin(3.0 * a + 0.2 * t + 2.0 * sin(5.0 * a - 0.4 * t)));
        spread *= (0.5 + 0.5 * sin(3.0 * a - sin(0.5 * t))) * grad;
        spread += (1.0 - spread) * grad;
        float streaks = spread * spread;
        streaks *= streaks;
        streaks = (spread + 3.0 * grad * grad + 4.0 * streaks * streaks);
        sky += 2.0 * streaks * skycol.horizon;
      }
    #endif
  }
  return sky;
}

// shooting star
vec3 nlRenderShootingStar(vec3 viewDir, vec3 FOG_COLOR, float t) {
  float h = t / (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD);
  float h0 = floor(h);
  t = (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD) * (h-h0);
  t = min(t/NL_SHOOTING_STAR_PERIOD, 1.0);
  float t0 = t*t;
  float t1 = 1.0-t0;
  t1 *= t1; t1 *= t1; t1 *= t1;

  float r = fract(sin(h0) * 43758.545313);
  float a = 6.2831*r;
  float cosa = cos(a);
  float sina = sin(a);
  vec2 uv = viewDir.xz * (6.0 + 4.0*r);
  uv = vec2(cosa*uv.x + sina*uv.y, -sina*uv.x + cosa*uv.y);
  uv.x += t1 - t;
  uv.x -= 2.0*r + 3.5;
  uv.y += viewDir.y * 3.0;

  float g = 1.0-min(abs((uv.x-0.95))*20.0, 1.0);
  float s = 1.0-min(abs(8.0*uv.y), 1.0);
  s *= s*s*smoothstep(-1.0+1.96*t1, 0.98-t, uv.x);
  s *= s*s*smoothstep(1.0, 0.98-t0, uv.x);
  s *= 1.0-t1;
  s *= 1.0-t0;
  s *= 0.7 + 16.0*g*g;
  s *= max(1.0-FOG_COLOR.r-FOG_COLOR.g-FOG_COLOR.b, 0.0);
  return s*vec3(0.8, 0.9, 1.0);
}

#endif