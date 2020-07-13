#include "r_shared.h"
#include <unordered_set>

struct dtexture_t
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
	int last_turbulent;
	int last_alias;
	int last_vertex;
	int last_index;
	int clear_color;
	std::unordered_set<void*> textured_index;
	std::vector<dtexture_t> textured;
	std::vector<dtexture_t> turbulent;
	std::vector<dtexture_t> alias;
	std::vector<float> vertices;
	std::vector<int> indices;
};

extern dlists_t d_lists;

extern qboolean d_uselists;

void D_AddTurbulentToLists (msurface_t* face, entity_t* entity);
void D_AddTexturedToLists (msurface_t* face, struct surfcache_s* cache, entity_t* entity);
void D_AddAliasToLists (aliashdr_t* aliashdr, trivertx_t* vertices, maliasskindesc_t* skindesc);
