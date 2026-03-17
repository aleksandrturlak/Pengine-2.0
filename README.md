# **Pengine**
A simple 3D game engine written in C++ and Vulkan.

## **Platforms:**
* Windows
* Linux

## **How to build**
### **Windows**
* Download GLFW 64-bit Windows binaries from https://www.glfw.org/download.html.
* Download Vulkan SDK from https://vulkan.lunarg.com and install it.
* Set up next environment variables: `GLFW_INCLUDE_PATH`, `GLFW_LIB_PATH`, `VULKAN_SPIRV_REFLECT_PATH`, `VULKAN_INCLUDE_PATH`, `VULKAN_LIB_PATH`, `VULKAN_BIN_PATH`.\
For example how I set it:\
`GLFW_INCLUDE_PATH = C:/GLFW/Includes`\
`GLFW_LIB_PATH = C:/GLFW/Lib`\
`VULKAN_INCLUDE_PATH = C:/VulkanSDK/1.3.268.0/Include`\
`VULKAN_LIB_PATH = C:/VulkanSDK/1.3.268.0/Lib`\
`VULKAN_SPIRV_REFLECT_PATH = C:/VulkanSDK/1.3.268.0/Source`\
`VULKAN_BIN_PATH = C:/VulkanSDK/1.3.268.0/Bin`
* Clone and run CMake.
```
git clone --recursive -b main https://github.com/Shturm0weak/Pengine-2.0.git
```
```
cd Pengine-2.0 && mkdir Build && cd Build && cmake ..
```
* Open Pengine (for VS 20XX `Pengine-2.0/Build/Pengine.sln`) and set up `SandBox` as a startup project. If you are using another IDE, the working directory should be set manually to `Pengine-2.0/SandBox/`.
* Try to build and run.

### **Linux**
* If there is no GLFW, so download GLFW for your linux distribution.
* Download Vulkan SDK from https://vulkan.lunarg.com.
* Install Vulkan SDK as shown in the documentation https://vulkan.lunarg.com/doc/view/latest/linux/getting_started.html.
* Set up next environment variables: `VULKAN_SPIRV_REFLECT_PATH`, `VULKAN_INCLUDE_PATH`, `VULKAN_LIB_PATH`, `VULKAN_BIN_PATH`.\
For example how I set it:\
`VULKAN_INCLUDE_PATH = /home/alexander/Vulkan/1.4.304.0/x86_64/include`\
`VULKAN_LIB_PATH = /home/alexander/Vulkan/1.4.304.0/x86_64/lib`\
`VULKAN_SPIRV_REFLECT_PATH = /home/alexander/Vulkan/1.4.304.0/x86_64/include`\
`VULKAN_BIN_PATH = /home/alexander/Vulkan/1.4.304.0/x86_64/bin`
Setting these variables can be done as a `.sh` script and run that script on startup by placing another `.sh` script in `/etc/profile.d` containing the line `source <path to your .sh script>`
* Clone and run CMake.
```
git clone --recursive -b main https://github.com/Shturm0weak/Pengine-2.0.git
```
```
cd Pengine-2.0 && mkdir Build && cd Build && cmake ..
```
* Set up the working directory to `Pengine-2.0/SandBox/`.
* Open Pengine and set up `SandBox` as a startup project.
* Try to build and run.

## **Graphics Features**
* GPU-Driven Rendering
* Ray Traced Shadows
* Ray Traced Reflections
* DDGI
* TAA
* Deferred Instanced Rendering
* Basic PBR
* Directional Lights
* Point Lights
* Spot Lights
* Normal Maps
* Cascade Shadow Maps
* Point Light Shadow Maps
* Spot Light Shadow Maps
* SSAO
* SSR
* SSS
* Parallax Occlusion Mapping
* Decals
* Bloom
* FXAA
* Tone Mapping
* Atmospheric Scattering
* Forward Order Dependent Transparency Rendering
* Multiple Viewport Rendering
* Assync Resource Loader
* Scriptable Materials
* Skeletal Animations
* Mesh LODs

## **Used Libraries**
* GLFW
* GLM
* Vulkan
* Entt
* FastGLTF
* ImGui
* Yaml-cpp
* Jolt
* FreeType
* MeshOptimizer

## **Screenshots**
<img width="2559" height="1374" alt="MultiViewports" src="https://github.com/user-attachments/assets/0bbcebaa-91fb-40ba-af93-8321af239cb6" />

![image](https://github.com/user-attachments/assets/ddfa34bb-5934-411c-b84f-588e5e4bd7f0)

![image](https://github.com/user-attachments/assets/053e7830-4217-4a9d-a578-dc31a80d86fc)

![image](https://github.com/user-attachments/assets/2c69fa6d-59a7-4ea6-a756-6ab668ef6704)

![image](https://github.com/user-attachments/assets/19903bde-e2ba-4439-acfb-994f382ca625)

<img width="2560" height="1377" alt="Materials" src="https://github.com/user-attachments/assets/69762984-ac18-4740-b5b6-4cbb67679883" />
<img width="2560" height="1377" alt="BistroNight" src="https://github.com/user-attachments/assets/b3cf6caa-4852-4ba8-a619-c14b0a2b8191" />

https://github.com/user-attachments/assets/f47cdb9f-c40e-4d38-88ea-fd54fad8a2f5

https://github.com/user-attachments/assets/1d6eee45-2424-4ba7-a65b-92672579031d

