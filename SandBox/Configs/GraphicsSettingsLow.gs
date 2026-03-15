HiZOcclusionCulling:
  IsEnabled: false
  DepthBias: 0.0199999996
SSAO:
  IsEnabled: false
  AoScale: 1
  Bias: 0.0250000004
  KernelSize: 16
  NoiseSize: 4
  Radius: 0.5
  ResolutionScale: 2
  ResolutionBlurScale: 2
CSM:
  IsEnabled: false
  Quality: 0
  CascadeCount: 2
  SplitFactor: 0.899999976
  MaxDistance: 15
  FogFactor: 0.200000003
  Filter: 2
  PcfRange: 1
  Biases:
    - 0
    - 0
  StabilizeCascades: true
SSS:
  IsEnabled: false
  ResolutionScale: 2
  ResolutionBlurScale: 2
  MaxRayDistance: 0.100000001
  MaxDistance: 5
  MaxSteps: 16
  MinThickness: -0.0125000002
  MaxThickness: 0.000150000007
PointLightShadows:
  IsEnabled: false
  AtlasQuality: 2
  FaceQuality: 2
SpotLightShadows:
  IsEnabled: false
  AtlasQuality: 2
  FaceQuality: 2
Bloom:
  IsEnabled: false
  MipCount: 10
  BrightnessThreshold: 1
  Intensity: 1
  ResolutionScale: 3
SSR:
  IsEnabled: false
  MaxDistance: 30
  Resolution: 0.300000012
  ResolutionBlurScale: 1
  ResolutionScale: 1
  StepCount: 10
  Thickness: 5
  BlurRange: 2
  BlurOffset: 1
  MipMultiplier: 1
  UseSkyBoxFallback: true
  Blur: 1
PostProcess:
  Gamma: 2.20000005
  ToneMapper: 1
Antialiasing:
  Mode: 0
  TAAJitterScale: 0.5
  TAAVarianceGamma: 1.5
  TAAMinBlendFactor: 0.850000024
  TAAMaxBlendFactor: 0.970000029
RayTracedShadows:
  DirectionalLight: false
  PointLight: false
  SpotLight: false
RayTracedReflections:
  IsRayTraced: false
DDGI:
  IsEnabled: false
  VisualizeProbes: true
  GridX: 32
  GridY: 10
  GridZ: 32
  ProbeSpacing: 4.5999999
  RaysPerProbe: 32
  FollowCamera: false
  FixedOrigin: [0, 0, 0]