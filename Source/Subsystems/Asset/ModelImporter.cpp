#include "ModelImporter.h"
#include "AssetData.h"
#include "../../Core/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>

namespace AstralEngine {

	std::shared_ptr<void> ModelImporter::Import(const std::string& filePath) {
		Logger::Trace("ModelImporter", "Loading ModelData from file: '{}'", filePath);

		auto modelData = std::make_shared<ModelData>(filePath);

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filePath,
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace |
			aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			Logger::Error("ModelImporter", "Assimp failed to load model '{}': {}", filePath, importer.GetErrorString());
			return nullptr;
		}

		// Process all meshes in the scene
		uint32_t vertexOffset = 0;
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
			const aiMesh* mesh = scene->mMeshes[i];
			if (!mesh->HasPositions()) continue;

			for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
				Vertex vertex;
				vertex.position = { mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z };
				if (mesh->HasNormals()) vertex.normal = { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z };
				if (mesh->HasTextureCoords(0)) vertex.texCoord = { mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y };
				if (mesh->HasTangentsAndBitangents()) {
					vertex.tangent = { mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z };
					vertex.bitangent = { mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z };
				}
				modelData->vertices.push_back(vertex);
			}

			for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
				const aiFace& face = mesh->mFaces[j];
				if (face.mNumIndices != 3) continue;
				for (unsigned int k = 0; k < face.mNumIndices; ++k) {
					modelData->indices.push_back(face.mIndices[k] + vertexOffset);
				}
			}

			vertexOffset += mesh->mNumVertices;
		}

		if (modelData->vertices.empty()) {
			Logger::Error("ModelImporter", "No valid geometry found in model '{}'", filePath);
			return nullptr;
		}

		// Calculate the bounding box for the entire model
		AABB boundingBox;
		for (const auto& vertex : modelData->vertices) {
			boundingBox.Extend(vertex.position);
		}
		modelData->boundingBox = boundingBox;

		modelData->isValid = true;
		modelData->name = std::filesystem::path(filePath).filename().string();

		Logger::Info("ModelImporter", "Successfully loaded model '{}' ({} vertices, {} indices)", 
					 modelData->name, modelData->vertices.size(), modelData->indices.size());

		return modelData;
	}

} // namespace AstralEngine
