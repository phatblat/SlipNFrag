#include "r_shared.h"

struct texturedverts_t
{
	float x, y, z;
};

struct texturedcoords_t
{
	float s, t;
};

struct textured_t
{
	void* key;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int last_vertex;
	std::vector<texturedverts_t> vertices;
	std::vector<texturedcoords_t> texcoords;
	int last_index;
	std::vector<int> indices;
};

struct lists_t
{
	int last_textured;
	std::vector<textured_t> textured;
};

extern lists_t r_lists;

extern qboolean r_uselists;

void R_AddFaceToLists (msurface_t* face);