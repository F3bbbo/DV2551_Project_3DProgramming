#include <string>
#include <SDL_keyboard.h>
#include <SDL_events.h>
#include <SDL_timer.h>
#pragma comment(lib,"SDL2.lib")
#include <type_traits> 
#include <assert.h>

#include "Renderer.h"
#include "DirectX12/Directx12Renderer.h"
#include "Mesh.h"
#include "Texture2D.h"
#include <math.h>
#include <memory>
#include <queue>

//#include "DirectX12/MeshDX12.h"
#include "MeshReader.h"
#include "Grid.h"
#include "IA.h"

//testing includes
#include <stdlib.h>     /* srand, rand */
#include <time.h>
#include <SimpleMath.h>

#include <process.h>
#include <mutex>
using namespace std;
DirectX12Renderer* renderer;
Grid* grid;
// flat scene at the application level...we don't care about this here.
// do what ever you want in your renderer backend.
// all these objects are loosely coupled, creation and destruction is responsibility
// of the testbench, not of the container objects
map<int, vector<Object>> object;


vector<std::shared_ptr<Mesh>> scene;
vector<std::shared_ptr<Material>> materials;
vector<std::shared_ptr<Technique>> techniques;
vector<std::shared_ptr<Texture2D>> textures;
vector<std::shared_ptr<Sampler2D>> samplers;

std::shared_ptr<VertexBuffer> pos;
std::shared_ptr<VertexBuffer> nor;
std::shared_ptr<VertexBuffer> uvs;

// forward decls
void updateScene();
void renderScene();

void updateGridList();
char gTitleBuff[256];
double gLastDelta = 0.0;

struct threadinfo
{
	int size;
	Object** data;
};

std::shared_ptr<Material> triangleMaterial;
std::shared_ptr<RenderState> triangleRS;
std::shared_ptr<Technique> triangleT;

struct int2
{
	int2(int x, int y) { this->x = x; this->y = y; };
	int x, y;
};

std::queue<int> idleThreads;
std::vector<int2> gridCellsToBeLoaded;

const int2 gridStart = { -5, -5 };
#define XOFFSET -5 + 0.5f * cellWidth
#define YOFFSET -5 + 0.5f * cellHeight

void updateDelta()
{
	#define WINDOW_SIZE 10
	static Uint64 start = 0;
	static Uint64 last = 0;
	static double avg[WINDOW_SIZE] = { 0.0 };
	static double lastSum = 10.0;
	static int loop = 0;

	last = start;
	start = SDL_GetPerformanceCounter();
	double deltaTime = (double)((start - last) * 1000.0 / SDL_GetPerformanceFrequency());
	// moving average window of WINDOWS_SIZE
	lastSum -= avg[loop];
	lastSum += deltaTime;
	avg[loop] = deltaTime;
	loop = (loop + 1) % WINDOW_SIZE;
	gLastDelta = (lastSum / WINDOW_SIZE);
};

// TOTAL_TRIS pretty much decides how many drawcalls in a brute force approach.
constexpr int TOTAL_TRIS = 100.0f;
// this has to do with how the triangles are spread in the screen, not important.
constexpr int TOTAL_PLACES = 2 * TOTAL_TRIS;
float xt[TOTAL_PLACES], yt[TOTAL_PLACES];

// lissajous points
typedef union { 
	struct { float x, y, z, w; };
	struct { float r, g, b, a; };
} float4;

typedef union { 
	struct { float x, y; };
	struct { float u, v; };
} float2;


void run() {

	SDL_Event windowEvent;
	while (true)
	{
		if (SDL_PollEvent(&windowEvent))
		{
			if (windowEvent.type == SDL_QUIT) break;
			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE) break;
		}
		updateGridList();

		/*for (int i = 0; i < gridCellsToBeLoaded.size(); i++)
			cout << gridCellsToBeLoaded[i].x << " " << gridCellsToBeLoaded[i].y << ", ";
		cout << endl;*/
		updateScene();
		renderScene();
	}
}

/*
 update positions of triangles in the screen changing a translation only
*/
void updateScene()
{

	// Check if new grids needs to be loaded and add them to a grid list.
	// Check if the list associated to a thread previously launched had finished by checking a fence, if the fence has been reached, add integers to the "idleThreads" queue indicating that a new thread can be launched with that command list.
	// Launch threads. Each thread is responisble for loading one grid cell, launch as many threads as available
		// A thread is given a vector of objects to be loaded. For each mesh the thread should:
			// Load the data from the file.
			// Create position, index , UVs and Normal vertex buffers with the appropriate resource state.
			// Each file to be loaded has a texture associated with it which needs to be loaded.
			// Upload the vertex data and texture to the GPU
			// When the mesh is uploaded, change the command list pointer that he mesh hold to the main threads command list.
			



	/*
	    For each mesh in scene list, update their position 
	*/
	//{
	//	static long long shift = 0;
	//	const int size = scene.size();
	//	for (int i = 0; i < size; i++)
	//	{
	//		const float4 trans { 
	//			xt[(int)(float)(i + shift) % (TOTAL_PLACES)], 
	//			yt[(int)(float)(i + shift) % (TOTAL_PLACES)], 
	//			i * (-1.0 / TOTAL_PLACES),
	//			0.0
	//		};
	//		scene[i]->txBuffer->setData(&trans, sizeof(trans), scene[i]->technique->getMaterial(), TRANSLATION);
	//	}
	//	// just to make them move...
	//	shift+=max(TOTAL_TRIS / 1000.0,TOTAL_TRIS / 100.0);
	//}
	return;
};


void renderScene()
{
	renderer->clearBuffer(CLEAR_BUFFER_FLAGS::COLOR | CLEAR_BUFFER_FLAGS::DEPTH);
	for (auto m : scene)
	{
		renderer->submit(m.get());
	}
	renderer->frame();
	renderer->present();
	updateDelta();
	sprintf(gTitleBuff, "DirectX12 - Dynamic scene loader test - %3.0lf", gLastDelta);
	renderer->setWinTitle(gTitleBuff);
}

int initialiseTestbench()
{
	//MeshReader TEST
	MeshReader mr(renderer);
	mr.LoadFromFile("Models/NewLowPolyTree.fbx","Models/PolyTreeTexture.png", scene);
	// triangle geometry:
	float4 triPos[3] = { { 0.0f,  1.0, 0.0f, 1.0f },{ 1.0, -1.0, 0.0f, 1.0f },{ -1.0, -1.0, 0.0f, 1.0f } };
	float4 triNor[3] = { { 0.0f,  0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f } };
	float2 triUV[3] =  { { 0.5f,  -0.99f },{ 1.49f, 1.1f },{ -0.51, 1.1f } };


	//Make material
	std::string shaderPath = renderer->getShaderPath();
	std::string shaderExtension = renderer->getShaderExtension();

	std::shared_ptr<Material> m = renderer->makeMaterial(shaderPath + "VertexShader" + shaderExtension);
	
	m->setShader(shaderPath + "VertexShader" + shaderExtension, Material::ShaderType::VS);
	m->setShader(shaderPath + "FragmentShader" + shaderExtension, Material::ShaderType::PS);

	std::string err;
	m->compileMaterial(err);

	materials.push_back(m);

	std::shared_ptr<RenderState> rs = renderer->makeRenderState();

	std::shared_ptr<Technique> t = renderer->makeTechnique(m, rs);

	techniques.push_back(t);

	for (unsigned int i = 0; i < scene.size(); i++)
	{
		scene[i]->technique = t;
	}

	//Allocate vertex buffers
	//pos = renderer->makeVertexBuffer(sizeof(triPos), VertexBuffer::DATA_USAGE::DONTCARE);
	//nor = renderer->makeVertexBuffer(sizeof(triNor), VertexBuffer::DATA_USAGE::DONTCARE);
	//uvs = renderer->makeVertexBuffer(sizeof(triUV), VertexBuffer::DATA_USAGE::DONTCARE);

	//Create mesh
	//std::shared_ptr<Mesh> mesh = renderer->makeMesh();
	//pos->setData(triPos, sizeof(triPos), 0);
	//mesh->addIAVertexBufferBinding(pos, 0, ARRAYSIZE(triPos), sizeof(float4), POS);
	//
	//nor->setData(triNor, sizeof(triNor), 0);
	//mesh->addIAVertexBufferBinding(nor, 0, ARRAYSIZE(triNor), sizeof(float4), NORM);

	//uvs->setData(triUV, sizeof(triUV), 0);
	//mesh->addIAVertexBufferBinding(uvs, 0, ARRAYSIZE(triUV), sizeof(float2), UVCOORD);

	//mesh->technique = t;

	//scene.push_back(mesh);


	//std::string definePos = "#define POSITION " + std::to_string(POSITION) + "\n";
	//std::string defineNor = "#define NORMAL " + std::to_string(NORMAL) + "\n";
	//std::string defineUV = "#define TEXTCOORD " + std::to_string(TEXTCOORD) + "\n";

	//std::string defineTX = "#define TRANSLATION " + std::to_string(TRANSLATION) + "\n";
	//std::string defineTXName = "#define TRANSLATION_NAME " + std::string(TRANSLATION_NAME) + "\n";
	//
	//std::string defineDiffCol = "#define DIFFUSE_TINT " + std::to_string(DIFFUSE_TINT) + "\n";
	//std::string defineDiffColName = "#define DIFFUSE_TINT_NAME " + std::string(DIFFUSE_TINT_NAME) + "\n";

	//std::string defineDiffuse = "#define DIFFUSE_SLOT " + std::to_string(DIFFUSE_SLOT) + "\n";

	//std::vector<std::vector<std::string>> materialDefs = {
	//	// vertex shader, fragment shader, defines
	//	// shader filename extension must be asked to the renderer
	//	// these strings should be constructed from the IA.h file!!!

	//	{ "VertexShader", "FragmentShader", definePos + defineNor + defineUV + defineTX + 
	//	   defineTXName + defineDiffCol + defineDiffColName }, 

	//	{ "VertexShader", "FragmentShader", definePos + defineNor + defineUV + defineTX + 
	//	   defineTXName + defineDiffCol + defineDiffColName }, 

	//	{ "VertexShader", "FragmentShader", definePos + defineNor + defineUV + defineTX + 
	//	   defineTXName + defineDiffCol + defineDiffColName + defineDiffuse	},

	//	{ "VertexShader", "FragmentShader", definePos + defineNor + defineUV + defineTX + 
	//	   defineTXName + defineDiffCol + defineDiffColName }, 
	//};

	//float degToRad = M_PI / 180.0;
	//float scale = (float)TOTAL_PLACES / 359.9;
	//for (int a = 0; a < TOTAL_PLACES; a++)
	//{
	//	xt[a] = 0.8f * cosf(degToRad * ((float)a/scale) * 3.0);
	//	yt[a] = 0.8f * sinf(degToRad * ((float)a/scale) * 2.0);
	//};

	//// triangle geometry:
	//float4 triPos[3] = { { 0.0f,  0.05, 0.0f, 1.0f },{ 0.05, -0.05, 0.0f, 1.0f },{ -0.05, -0.05, 0.0f, 1.0f } };
	//float4 triNor[3] = { { 0.0f,  0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f } };
	//float2 triUV[3] =  { { 0.5f,  -0.99f },{ 1.49f, 1.1f },{ -0.51, 1.1f } };

	//// load Materials.
	//std::string shaderPath = renderer->getShaderPath();
	//std::string shaderExtension = renderer->getShaderExtension();
	//float diffuse[4][4] = {
	//	0.0,0.0,1.0,1.0,
	//	0.0,1.0,0.0,1.0,
	//	1.0,1.0,1.0,1.0,
	//	1.0,0.0,0.0,1.0
	//};

	//for (int i = 0; i < materialDefs.size(); i++)
	//{
	//	// set material name from text file?
	//	Material* m = renderer->makeMaterial("material_" + std::to_string(i));
	//	m->setShader(shaderPath + materialDefs[i][0] + shaderExtension, Material::ShaderType::VS);
	//	m->setShader(shaderPath + materialDefs[i][1] + shaderExtension, Material::ShaderType::PS);

	//	m->addDefine(materialDefs[i][2], Material::ShaderType::VS);
	//	m->addDefine(materialDefs[i][2], Material::ShaderType::PS);

	//	std::string err;
	//	m->compileMaterial(err);

	//	// add a constant buffer to the material, to tint every triangle using this material
	//	m->addConstantBuffer(DIFFUSE_TINT_NAME, DIFFUSE_TINT);
	//	// no need to update anymore
	//	// when material is bound, this buffer should be also bound for access.

	//	m->updateConstantBuffer(diffuse[i], 4 * sizeof(float), DIFFUSE_TINT);
	//	
	//	materials.push_back(m);
	//}

	//// one technique with wireframe
	//RenderState* renderState1 = renderer->makeRenderState();
	//renderState1->setWireFrame(true);

	//// basic technique
	//techniques.push_back(renderer->makeTechnique(materials[0], renderState1));
	//techniques.push_back(renderer->makeTechnique(materials[1], renderer->makeRenderState()));
	//techniques.push_back(renderer->makeTechnique(materials[2], renderer->makeRenderState()));
	//techniques.push_back(renderer->makeTechnique(materials[3], renderer->makeRenderState()));

	//// create texture
	//Texture2D* fatboy = renderer->makeTexture2D();
	//fatboy->loadFromFile("../assets/textures/fatboy.png");
	//Sampler2D* sampler = renderer->makeSampler2D();
	//sampler->setWrap(WRAPPING::REPEAT, WRAPPING::REPEAT);
	//fatboy->sampler = sampler;

	//textures.push_back(fatboy);
	//samplers.push_back(sampler);

	//// pre-allocate one single vertex buffer for ALL triangles
	//pos = renderer->makeVertexBuffer(TOTAL_TRIS * sizeof(triPos), VertexBuffer::DATA_USAGE::STATIC);
	//nor = renderer->makeVertexBuffer(TOTAL_TRIS * sizeof(triNor), VertexBuffer::DATA_USAGE::STATIC);
	//uvs = renderer->makeVertexBuffer(TOTAL_TRIS * sizeof(triUV), VertexBuffer::DATA_USAGE::STATIC);

	//// Create a mesh array with 3 basic vertex buffers.
	//for (int i = 0; i < TOTAL_TRIS; i++) {

	//	Mesh* m = renderer->makeMesh();

	//	constexpr auto numberOfPosElements = std::extent<decltype(triPos)>::value;
	//	size_t offset = i * sizeof(triPos);
	//	pos->setData(triPos, sizeof(triPos), offset);
	//	m->addIAVertexBufferBinding(pos, offset, numberOfPosElements, sizeof(float4), POSITION);

	//	constexpr auto numberOfNorElements = std::extent<decltype(triNor)>::value;
	//	offset = i * sizeof(triNor);
	//	nor->setData(triNor, sizeof(triNor), offset);
	//	m->addIAVertexBufferBinding(nor, offset, numberOfNorElements, sizeof(float4), NORMAL);

	//	constexpr auto numberOfUVElements = std::extent<decltype(triUV)>::value;
	//	offset = i * sizeof(triUV);
	//	uvs->setData(triUV, sizeof(triUV), offset);
	//	m->addIAVertexBufferBinding(uvs, offset, numberOfUVElements , sizeof(float2), TEXTCOORD);

	//	// we can create a constant buffer outside the material, for example as part of the Mesh.
	//	m->txBuffer = renderer->makeConstantBuffer(std::string(TRANSLATION_NAME), TRANSLATION);
	//	
	//	m->technique = techniques[ i % 4];
	//	if (i % 4 == 2)
	//		m->addTexture(textures[0], DIFFUSE_SLOT);

	//	scene.push_back(m);
	//}
	return 0;
}

void shutdown() {
	// shutdown.
	// delete dynamic objects
	//for (auto m : materials)
	//{
	//	delete(m);
	//}
	//for (auto t : techniques)
	//{
	//	delete(t);
	//}
	//for (auto m : scene)
	//{
	//	delete(m);
	//};
	//assert(pos->refCount() == 0);
	//delete pos;
	//assert(nor->refCount() == 0);
	//delete nor;
	//assert(uvs->refCount() == 0);
	//delete uvs;
	//
	//for (auto s : samplers)
	//{
	//	delete s;
	//}

	//for (auto t : textures)
	//{
	//	delete t;
	//}
	renderer->shutdown();
};
unsigned int __stdcall  threadfunctionloadingdata(void* data)
{
	int i = 0;
	threadinfo * threadinformation = (threadinfo*) data;
	//	std::cout << threadinformation->data[i]->position.x << std::endl;
	//	Object** testdata = (Object**)data;
//	std::cout << testdata[0]->position.x << std::endl;
	int x = 0;
	int y = 0;
	float4 triNor[3] = { { 0.0f,  0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 0.0f } };
	int triInd[3] = { 0, 1, 2 };
	float2 triUV[3] = { { 0.5f,  -0.99f },{ 1.49f, 1.1f },{ -0.51, 1.1f } };
	int amount = threadinformation->size;
	for (int i = 0; i < amount; i++)
	{
	//	std::shared_ptr<ConstantBuffer> cbmesh;
		// triangle geometry:
		float4 triPos[3] = { { x * cellWidth,  0.05, y * cellHeight, 1.0f },{ x * cellWidth + 0.05, -0.05, y * cellHeight, 1.0f },{ x * cellWidth - 0.05, -0.05, y * cellHeight, 1.0f } };

		std::shared_ptr<VertexBuffer> trianglePos = renderer->makeVertexBuffer(sizeof(triPos), VertexBuffer::DATA_USAGE::DONTCARE);
		std::shared_ptr<VertexBuffer> triangleNor = renderer->makeVertexBuffer(sizeof(triNor), VertexBuffer::DATA_USAGE::DONTCARE);
		std::shared_ptr<VertexBuffer> triangleUvs = renderer->makeVertexBuffer(sizeof(triUV), VertexBuffer::DATA_USAGE::DONTCARE);
		std::shared_ptr<VertexBuffer> triangleInd = renderer->makeVertexBuffer(sizeof(triInd), VertexBuffer::DATA_USAGE::DONTCARE);

		//Create mesh
		std::shared_ptr<Mesh> mesh = renderer->makeMesh(1);
		trianglePos->setData(triPos, sizeof(triPos), 0);
		mesh->addIAVertexBufferBinding(trianglePos, 0, ARRAYSIZE(triPos), sizeof(float4), POS);

		triangleNor->setData(triNor, sizeof(triNor), 0);
		mesh->addIAVertexBufferBinding(triangleNor, 0, ARRAYSIZE(triNor), sizeof(float4), NORM);

		triangleUvs->setData(triUV, sizeof(triUV), 0);
		mesh->addIAVertexBufferBinding(triangleUvs, 0, ARRAYSIZE(triUV), sizeof(float2), UVCOORD);

		triangleInd->setData(triInd, sizeof(triInd), 0);
		mesh->addIAVertexBufferBinding(triangleInd, 0, ARRAYSIZE(triInd), sizeof(float), INDEXBUFF);

		mesh->technique = triangleT;
		mesh->setRotation(threadinformation->data[i]->rotation);
		mesh->setScale(threadinformation->data[i]->scale);
		mesh->setTranslation(threadinformation->data[i]->position);
	//	mesh->setCBuffer(cbmesh);
		scene.push_back(mesh);
	}
	
		return 1;
}
void fillCell(int x, int y, int amount)
{
	srand(time(NULL));

	float randNumb;
	for (int i = 0; i < amount; i++)
	{
		randNumb = (float)(rand() % 1000) / 1000.f;
		float pos[3] = { x * cellWidth + randNumb, 1, y * cellWidth + randNumb };
		float scale[3] = { 1, 1, 1 };
		float rot[3] = { 0, 0, 0 };
		Object* object = new Object(pos, scale, rot, "Models/PolyTreeTexture.png");
		grid->addMesh(x, y, object);
	}

}


void createThreads()
{

	//    std::cout << (*grid)[0][0]->objectList.size(); 
	//    std::cout << (*grid)[0][1]->objectList.size(); 
	//    std::cout << (*grid)[0][2]->objectList.size(); 
//	std::vector<Object*> data1 = (*grid)[0][0]->objectList;
//	std::vector<Object*> data2 = (*grid)[0][1]->objectList;
//	std::vector<Object*> data3 = (*grid)[0][2]->objectList;
	//    std::cout << (*grid)[0][0]->objectList[0]->position.x << std::endl; 
	threadinfo data1 = { (*grid)[0][0]->objectList.size(),(*grid)[0][0]->objectList.data() };
	HANDLE Thread1;
	Thread1 = (HANDLE)_beginthreadex(0, 0, &threadfunctionloadingdata, &data1, 0, 0);
	WaitForSingleObject(Thread1, INFINITE);
	CloseHandle(Thread1);
}
void fillGrid()
{
	for (int x = 0; x < 10; x++)
	{
		for (int y = 0; y < 10; y++)
		{
			fillCell(x, y, 2);
		}
		cout << x << endl;
	}
}

void createGlobalData()
{
	std::string shaderPath = renderer->getShaderPath();
	std::string shaderExtension = renderer->getShaderExtension();

	triangleMaterial = renderer->makeMaterial(shaderPath + "VertexShader" + shaderExtension);

	triangleMaterial->setShader(shaderPath + "VertexShader" + shaderExtension, Material::ShaderType::VS);
	triangleMaterial->setShader(shaderPath + "FragmentShader" + shaderExtension, Material::ShaderType::PS);

	std::string err;
	triangleMaterial->compileMaterial(err);

	materials.push_back(triangleMaterial);

	triangleRS = renderer->makeRenderState();
	triangleT = renderer->makeTechnique(triangleMaterial, triangleRS);

	techniques.push_back(triangleT);
}

void updateGridList()
{
	Vector2 camPos = { renderer->camera->getPosition().x, renderer->camera->getPosition().z };
	int xStartDist = min(max(0, (int)camPos.x - LOADINGTHRESHOLD), WWidth);
	int yStartDist = min(max(0, (int)camPos.y - LOADINGTHRESHOLD), WWidth);
	int xEndDist = min(max(0, (int)camPos.x + LOADINGTHRESHOLD), WWidth);
	int yEndDist = min(max(0, (int)camPos.y + LOADINGTHRESHOLD), WWidth);

	//Check if cells needs to be loaded
	for (int x = xStartDist; x < xEndDist; x++)
	{
		for (int y = yStartDist; y < yEndDist; y++)
		{
			if ((*grid)[x][y]->status == NOT_LOADED)
			{
				(*grid)[x][y]->status = PENDING_LOAD;
				gridCellsToBeLoaded.push_back(int2(x, y));
			}
		}
	}
	
	//Remove cells that should had been loaded but didn't even start loading in time.
	for (int i = 0; i < gridCellsToBeLoaded.size(); i++)
	{
		if (gridCellsToBeLoaded[i].x < xStartDist || gridCellsToBeLoaded[i].x > xEndDist || gridCellsToBeLoaded[i].y < yStartDist || gridCellsToBeLoaded[i].y > yEndDist)
		{
			(*grid)[gridCellsToBeLoaded[i].x][gridCellsToBeLoaded[i].y]->status = NOT_LOADED;
			gridCellsToBeLoaded.erase(gridCellsToBeLoaded.begin() + i);
		}
	}
}
#undef main
int main(int argc, char *argv[])
{
	renderer = static_cast<DirectX12Renderer*>(Renderer::makeRenderer(Renderer::BACKEND::DX12));
	renderer->initialize(800, 600);
	renderer->setWinTitle("DirectX12 - Dynamic scene loader test");
	renderer->setClearColor(0.0, 0.1, 0.1, 1.0);
	
	createGlobalData();
	grid = new Grid();
	grid->createGrid(WWidth, HHeight);
	fillGrid();
	createThreads();
	//(*grid)[0].size();
	//Vector3 pos = (*grid)[0][0]->objectList[0]->position;
	//initialiseTestbench();
	run();
	shutdown();
	return 0;
};