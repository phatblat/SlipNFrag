#include "r_shared.h"

struct dtexturedverts_t
{
	float x, y, z;
};

struct dtexturedcoords_t
{
	float s, t;
};

struct dtextured_t
{
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int last_vertex;
	std::vector<dtexturedverts_t> vertices;
	std::vector<dtexturedcoords_t> texcoords;
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

void D_AddFaceToLists (msurface_t* face, struct surfcache_s* cache);
