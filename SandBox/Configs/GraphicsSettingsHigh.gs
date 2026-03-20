HiZOcclusionCulling:
  IsEnabled: false
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
  MipCount: 9
  BrightnessThreshold: 1
  Intensity: 1
  ResolutionScale: 3
SSR:
  IsEnabled: true
  MaxDistance: 100
  Resolution: 1
  ResolutionBlurScale: 3
  ResolutionScale: 2
  StepCount: 20
  Thickness: 500
  BlurRange: 1
  BlurOffset: 1
  MipMultiplier: 0
  UseSkyBoxFallback: true
  Blur: 1
PostProcess:
  Gamma: 2.20000005
  ToneMapper: 1
Antialiasing:
  Mode: 2
  TAAJitterScale: 0.5
  TAAVarianceGamma: 0.5
  TAAMinBlendFactor: 0.850000024
  TAAMaxBlendFactor: 0.970000029
RayTracedShadows:
  DirectionalLight: false
  PointLight: false
  SpotLight: false
RayTracedReflections:
  IsRayTraced: true
DDGI:
  IsEnabled: false
  VisualizeProbes: false
  GridX: 10
  GridY: 2
  GridZ: 10
  ProbeSpacing: 3
  RaysPerProbe: 96
  FollowCamera: false
  FixedOrigin: [0, 3, 0]