HiZOcclusionCulling:
  IsEnabled: true
  DepthBias: 0.0199999996
SSAO:
  IsEnabled: true
  AoScale: 1
  Bias: 0.0500000007
  KernelSize: 16
  NoiseSize: 64
  Radius: 0.5
  ResolutionScale: 1
  ResolutionBlurScale: 1
CSM:
  IsEnabled: true
  Quality: 1
  CascadeCount: 4
  SplitFactor: 0.899999976
  MaxDistance: 500
  FogFactor: 0.200000003
  Filter: 2
  PcfRange: 2
  Biases:
    - 0.0189999994
    - 0.0970000029
    - 0.224999994
    - 0.699999988
  StabilizeCascades: true
SSS:
  IsEnabled: true
  ResolutionScale: 2
  ResolutionBlurScale: 2
  MaxRayDistance: 0.100000001
  MaxDistance: 50
  MaxSteps: 16
  MinThickness: -0.0125000002
  MaxThickness: -0.00999999978
PointLightShadows:
  IsEnabled: true
  AtlasQuality: 3
  FaceQuality: 3
SpotLightShadows:
  IsEnabled: true
  AtlasQuality: 3
  FaceQuality: 3
Bloom:
  IsEnabled: true
  MipCount: 10
  BrightnessThreshold: 1
  Intensity: 1
  ResolutionScale: 3
SSR:
  IsEnabled: true
  MaxDistance: 100
  Resolution: 1
  ResolutionBlurScale: 4
  ResolutionScale: 3
  StepCount: 20
  Thickness: 0.209999993
  BlurRange: 1
  BlurOffset: 1
  MipMultiplier: 0
  UseSkyBoxFallback: true
  Blur: 0
PostProcess:
  Gamma: 2.20000005
  ToneMapper: 1
  FXAA: true
RayTracedShadows:
  DirectionalLight: true
  PointLight: true
  SpotLight: true
RayTracedReflections:
  IsRayTraced: true