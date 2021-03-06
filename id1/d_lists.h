#include "r_shared.h"

struct dsurface_t
{
	void* surface;
	void* entity;
	int created;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int first_vertex;
	int count;
	float origin_x;
	float origin_y;
	float origin_z;
};

struct dspritedata_t
{
	void* frame;
	int width;
	int height;
	int size;
	std::vector<unsigned char> data;
	int first_vertex;
	int count;
};

struct dturbulent_t
{
	void* texture;
	int width;
	int height;
	int size;
	unsigned char* data;
	int first_vertex;
	int count;
	float origin_x;
	float origin_y;
	float origin_z;
};

struct dalias_t
{
	int width;
	int height;
	int size;
	unsigned char* data;
	std::vector<unsigned char> colormap;
	qboolean is_host_colormap;
	trivertx_t* vertices;
	int vertex_count;
	int first_attribute;
	int first_index16;
	int first_index32;
	int count;
	float transform[3][4];
};

struct dparticle_t
{
	byte color;
	int first_index16;
	int first_index32;
	int count;
};

struct dsky_t
{
	float top;
	float left;
	float right;
	float bottom;
	int first_vertex;
	int count;
};

struct dlists_t
{
	int last_surface;
	int last_sprite;
	int last_turbulent;
	int last_alias;
	int last_viewmodel;
	int last_particle;
	int last_sky;
	int last_textured_vertex;
	int last_textured_attribute;
	int last_colormapped_vertex;
	int last_colormapped_attribute;
	int last_colormapped_index16;
	int last_colormapped_index32;
	int last_colored_vertex;
	int last_colored_index16;
	int last_colored_index32;
	int clear_color;
	std::vector<dsurface_t> surfaces;
	std::vector<dspritedata_t> sprites;
	std::vector<dturbulent_t> turbulent;
	std::vector<dalias_t> alias;
	std::vector<dalias_t> viewmodel;
	std::vector<dparticle_t> particles;
	std::vector<dsky_t> sky;
	std::vector<float> textured_vertices;
	std::vector<float> textured_attributes;
	std::vector<float> colormapped_vertices;
	std::vector<float> colormapped_attributes;
	std::vector<uint16_t> colormapped_indices16;
	std::vector<uint32_t> colormapped_indices32;
	std::vector<float> colored_vertices;
	std::vector<uint16_t> colored_indices16;
	std::vector<uint32_t> colored_indices32;
};

extern dlists_t d_lists;

extern qboolean d_uselists;
extern qboolean d_awayfromviewmodel;

void D_AddSurfaceToLists (msurface_t* face, struct surfcache_s* cache, entity_t* entity, qboolean created);
void D_AddSpriteToLists (vec5_t* pverts, spritedesc_t* spritedesc);
void D_AddTurbulentToLists (msurface_t* face, entity_t* entity);
void D_AddAliasToLists (aliashdr_t* aliashdr, maliasskindesc_t* skindesc, byte* colormap, trivertx_t* vertices);
void D_AddViewModelToLists (aliashdr_t* aliashdr, maliasskindesc_t* skindesc, byte* colormap, trivertx_t* vertices);
void D_AddParticleToLists (particle_t* part);
void D_AddSkyToLists (surf_t* surf);
