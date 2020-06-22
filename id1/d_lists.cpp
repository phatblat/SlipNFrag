#include "quakedef.h"
#include "d_local.h"
#include "d_lists.h"

dlists_t d_lists { -1 };

qboolean d_uselists = false;

void D_AddFaceToList (surf_t* surface, surfcache_t* cache)
{
	auto pface = (msurface_t*)surface->data;
	if (pface->numedges < 3)
	{
		return;
	}
	if (d_lists.textured.size() == 0 || (d_lists.textured[d_lists.last_textured].key != (void*)cache))
	{
		d_lists.last_textured++;
		if (d_lists.last_textured >= d_lists.textured.size())
		{
			d_lists.textured.emplace_back();
		}
		auto& entry = d_lists.textured[d_lists.last_textured];
		entry.key = 0;
		entry.width = cache->width;
		entry.height = cache->height;
		int size = entry.width * entry.height;
		entry.size = size;
		if (size > entry.data.size())
		{
			entry.data.resize(size);
		}
		memcpy(entry.data.data(), cache->data, size);
		entry.last_vertex = -1;
		entry.last_index = -1;
	}
	auto& entry = d_lists.textured[d_lists.last_textured];
	auto model = surface->entity->model;
	auto edgeindex = pface->firstedge;
	auto texture = pface->texinfo;
	for (auto i = 0; i < pface->numedges; i++)
	{
		auto edge = model->surfedges[edgeindex];
		mvertex_t* vertex;
		if (edge >= 0)
		{
			vertex = &model->vertexes[model->edges[edge].v[0]];
		}
		else
		{
			vertex = &model->vertexes[model->edges[-edge].v[1]];
		}
		auto u = vertex->position[0] * texture->vecs[0][0] + vertex->position[1] * texture->vecs[0][1] + vertex->position[2] * texture->vecs[0][2] + texture->vecs[0][3];
		auto v = vertex->position[0] * texture->vecs[1][0] + vertex->position[1] * texture->vecs[1][1] + vertex->position[2] * texture->vecs[1][2] + texture->vecs[1][3];
		entry.last_vertex++;
		if (entry.last_vertex >= entry.vertices.size())
		{
			entry.vertices.emplace_back();
		}
		auto& item = entry.vertices[entry.last_vertex];
		item.x = vertex->position[0];
		item.y = vertex->position[1];
		item.z = vertex->position[2];
		item.u = u;
		item.v = v;
		edgeindex++;
	}
}
