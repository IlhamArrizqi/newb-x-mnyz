# Newb X Manyzz (basis newb-x-mcbe-main 1.26.50)

Disalin dari manyzz: nilai config.h, subpack (awan/aurora/night), aurora 3D, blackhole + galaxy End,
awan rounded smooth, lava wave, glow item di tangan, perataan dawn, fix rand() noise, tekstur glow, ikon.

Sengaja TIDAK disalin (kode manyzz berbasis MC 1.26.30 / mcbe lama):
- wave.h (indeks tekstur beda)        - detection.h (DimensionID/Day)
- uniform Day/DimensionID di vertex   - tool/*.py, requirements.txt, workflow, tool/data (materials 1.26.10)
- SunMoon/vertex.sc (opsional, lihat jawaban)

Diperbaiki agar jalan di mcbe:
- noise() -> nlCloudSmoothNoise() (bentrok dengan intrinsic HLSL)
- NL_LAVA_WAVE tidak lagi terkunci di dalam #ifdef NL_LAVA_NOISE
- Subpack NIGHT_VISION dibuat ulang (makro aslinya tidak dipakai)
- Aurora tetap pakai versi mcbe (dayFactor), bukan FogColor
- EndSky: nlRenderGalaxy / renderBlackhole dipakai di atas sky.h mcbe
BELUM DIKOMPILASI: jalankan build (lazurite) lalu cek error shader.
