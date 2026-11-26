$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_position, v_extra

#include <bgfx_shader.sh>
#include <newb/main.sh>

uniform vec4 ViewPositionAndTime;

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  diffuse.rgb += glow;

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);
      diffuse.rgb *= 1.0 - 0.6*mask;
      diffuse.rgb += v_refl.rgb*mask;
    }
  }

  // rain drop ripples - only on flat upward faces (not water/plants)
  #if defined(NL_RAIN_RIPPLE) && !defined(ALPHA_TEST) && !defined(RENDER_AS_BILLBOARDS)
    if (v_position.w > 0.0 && v_extra.b < 0.9 && v_extra.r > 0.95) {
      float flatTest = abs(dFdx(v_extra.g)) + abs(dFdy(v_extra.g));
      if (flatTest < 0.0004) {
        // day light (sky brightness) or torch light
        float dayLight = clamp(dot(v_fog.rgb, vec3(0.5,0.5,0.5)), 0.0, 1.0);
        float brightness = max(dayLight, pow(v_lightmapUV.x, 1.4)*0.8);
        vec4 ripple = nlRainRippleShade(v_position.xz, ViewPositionAndTime.w, v_position.w, v_fog.a, brightness);
        diffuse.rgb *= 1.0 - ripple.a;
        diffuse.rgb += ripple.rgb;
      }
    }
  #endif

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  diffuse.rgb = colorCorrection(diffuse.rgb);

  gl_FragColor = diffuse;
}
