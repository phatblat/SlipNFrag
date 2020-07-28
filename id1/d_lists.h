#include "r_shared.h"
#include <unordered_set>

struct dsurface_t
{
	void* surface;
	void* entity;
	int created;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int first_index;
	int count;
};

struct dturbulent_t
{
	void* texture;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int first_index;
	int count;
};

struct dalias_t
{
	void* model;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	std::vector<unsigned char> colormap;
	int is_host_colormap;
	int first_index;
	int count;
};

struct dparticle_t
{
	byte color;
	int first_index;
	int count;
};

struct dsky_t
{
	int first_index;
	int count;
};

struct dlists_t
{
	int last_surface;
	int last_turbulent;
	int last_alias;
	int last_particle;
	int last_sky;
	int last_textured_vertex;
	int last_textured_index;
	int last_colormapped_vertex;
	int last_colormapped_index;
	int last_colored_vertex;
	int last_colored_index;
	int clear_color;
	std::vector<dsurface_t> surfaces;
	std::vector<dturbulent_t> turbulent;
	std::vector<dalias_t> alias;
	std::vector<dparticle_t> particles;
	std::vector<dsky_t> sky;
	std::vector<float> textured_vertices;
	std::vector<uint32_t> textured_indices;
	std::vector<float> colormapped_vertices;
	std::vector<uint32_t> colormapped_indices;
	std::vector<float> colored_vertices;
	std::vector<uint32_t> colored_indices;
};

extern dlists_t d_lists;

extern qboolean d_uselists;

void D_AddSurfaceToLists (msurface_t* face, struct surfcache_s* cache, entity_t* entity, qboolean created);
void D_AddTurbulentToLists (msurface_t* face, entity_t* entity);
void D_AddAliasToLists (aliashdr_t* aliashdr, trivertx_t* vertices, maliasskindesc_t* skindesc, byte* colormap);
void D_AddParticleToLists (particle_t* part);
void D_AddSkyToLists (msurface_t* face, entity_t* entity);
