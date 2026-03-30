#pragma once

namespace Pengine::FileFormats
{
	inline const char* BaseMat()
	{
		return ".basemat";
	}

	inline const char* Mat()
	{
		return ".mat";
	}

	inline const char* Png()
	{
		return ".png";
	}

	inline const char* Jpeg()
	{
		return ".jpeg";
	}

	inline const char* Jpg()
	{
		return ".jpg";
	}

	inline const char* Dds()
	{
		return ".dds";
	}

	inline const char* Tga()
	{
		return ".tga";
	}

	inline const char* Obj()
	{
		return ".obj";
	}

	inline const char* Gltf()
	{
		return ".gltf";
	}

	inline const char* Glb()
	{
		return ".glb";
	}

	inline const char* Fbx()
	{
		return ".fbx";
	}

	inline const char* Mesh()
	{
		return ".mesh";
	}

	inline const char* Meta()
	{
		return ".meta";
	}

	inline const char* Spv()
	{
		return ".spv";
	}

	inline const char* Refl()
	{
		return ".refl";
	}

	inline const char* Prefab()
	{
		return ".prefab";
	}

	inline const char* Scene()
	{
		return ".scene";
	}

	inline const char* GraphicsSettings()
	{
		return ".gs";
	}

	inline const char* Frag()
	{
		return ".frag";
	}

	inline const char* Vert()
	{
		return ".vert";
	}

	inline const char* Geom()
	{
		return ".geom";
	}

	inline const char* Comp()
	{
		return ".comp";
	}

	inline const char* Anim()
	{
		return ".anim";
	}

	inline const char* Skeleton()
	{
		return ".skeleton";
	}

	inline const char* Track()
	{
		return ".track";
	}

	inline const char* Wav()
	{
		return ".wav";
	}

	inline const char* Mp3()
	{
		return ".mp3";
	}

	inline const char* Flac()
	{
		return ".flac";
	}

	inline const char* Ogg()
	{
		return ".ogg";
	}

	inline bool IsAudio(const std::string& fileFormat)
	{
		return fileFormat == Wav()
			|| fileFormat == Mp3()
			|| fileFormat == Flac()
			|| fileFormat == Ogg();
	}

	inline bool IsTexture(const std::string& fileFormat)
	{
		return fileFormat == Png()
			|| fileFormat == Jpeg()
			|| fileFormat == Jpg()
			|| fileFormat == Dds()
			|| fileFormat == Tga();
	}

	inline bool IsShader(const std::string& fileFormat)
	{
		return fileFormat == Frag()
			|| fileFormat == Vert()
			|| fileFormat == Geom()
			|| fileFormat == Comp();
	}

	inline bool IsMeshIntermediate(const std::string& fileFormat)
	{
		return fileFormat == Obj()
			|| fileFormat == Gltf()
			|| fileFormat == Glb()
			|| fileFormat == Fbx();
	}

	inline bool IsAsset(const std::string& fileFormat)
	{
		if (IsTexture(fileFormat)
			|| IsShader(fileFormat)
			|| IsAudio(fileFormat)
			|| fileFormat == BaseMat()
			|| fileFormat == Mat()
			|| fileFormat == Mesh()
			|| fileFormat == Prefab()
			|| fileFormat == GraphicsSettings()
			|| fileFormat == Anim()
			|| fileFormat == Skeleton()
			|| fileFormat == Scene()
			|| fileFormat == Track())
		{
			return true;
		}

		return false;
	}

}
