#include "r_shared.h"

struct dtextured_t
{
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int first_index;
	int count;
};

struct dlists_t
{
	int last_textured;
	int last_vertex;
	int last_index;
	std::vector<dtextured_t> textured;
	std::vector<float> vertices;
	std::vector<int> indices;
};

extern dlists_t d_lists;

extern qboolean d_uselists;

void D_AddFaceToLists (msurface_t* face, struct surfcache_s* cache, entity_t* entity);
