#include "r_shared.h"

struct dtexturedverts_t
{
	float x, y, z;
	float u, v;
};

struct dtextured_t
{
	void* key;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int last_vertex;
	std::vector<dtexturedverts_t> vertices;
	int last_index;
	std::vector<int> indices;
};

struct dlists_t
{
	int last_textured;
	std::vector<dtextured_t> textured;
};

extern dlists_t d_lists;

extern qboolean d_uselists;

void D_AddFaceToList (surf_t* surface, surfcache_t* cache);
