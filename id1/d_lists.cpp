#include "quakedef.h"
#include "d_lists.h"
#include "d_local.h"

dlists_t d_lists { -1 };

qboolean d_uselists = false;

void D_AddFaceToLists (msurface_t* face, surfcache_t* cache)
{
	if (face->numedges < 3)
	{
		return;
	}
	auto texinfo = face->texinfo;
	d_lists.last_textured++;
	if (d_lists.last_textured >= d_lists.textured.size())
	{
		d_lists.textured.emplace_back();
	}
	auto& entry = d_lists.textured[d_lists.last_textured];
	entry.width = cache->width;
	entry.height = cache->height;
	entry.size = entry.width * entry.height;
	if (entry.size > entry.data.size())
	{
		entry.data.resize(entry.size);
	}
	memcpy(entry.data.data(), cache->data, entry.size);
	entry.last_vertex = -1;
	entry.last_index = -1;
	auto next_front = entry.last_vertex + 1;
	auto next_back = next_front + face->numedges - 1;
	auto edgeindex = face->firstedge;
	for (auto i = 0; i < face->numedges; i++)
	{
		auto edge = currententity->model->surfedges[edgeindex];
		mvertex_t* vertex;
		if (edge >= 0)
		{
			vertex = &currententity->model->vertexes[currententity->model->edges[edge].v[0]];
		}
		else
		{
			vertex = &currententity->model->vertexes[currententity->model->edges[-edge].v[1]];
		}
		auto x = vertex->position[0];
		auto y = vertex->position[1];
		auto z = vertex->position[2];
		auto s = x * texinfo->vecs[0][0] + y * texinfo->vecs[0][1] + z * texinfo->vecs[0][2] + texinfo->vecs[0][3];
		auto t = x * texinfo->vecs[1][0] + y * texinfo->vecs[1][1] + z * texinfo->vecs[1][2] + texinfo->vecs[1][3];
		entry.last_vertex++;
		if (entry.last_vertex >= entry.vertices.size())
		{
			entry.vertices.emplace_back();
			entry.texcoords.emplace_back();
		}
		auto& vert = entry.vertices[entry.last_vertex];
		vert.x = -x;
		vert.y = z;
		vert.z = y;
		auto& tex = entry.texcoords[entry.last_vertex];
		tex.s = (s - face->texturemins[0]) / face->extents[0];
		tex.t = (t - face->texturemins[1]) / face->extents[1];
		edgeindex++;
	}
	qboolean use_back = false;
	for (auto i = 0; i < face->numedges - 2; i++)
	{
		int v0;
		int v1;
		int v2;
		if (use_back)
		{
			v0 = next_front;
			v2 = next_back;
			next_back--;
			v1 = next_back;
		}
		else
		{
			v0 = next_front;
			next_front++;
			v1 = next_front;
			v2 = next_back;
		}
		use_back = !use_back;
		entry.last_index++;
		if (entry.last_index >= entry.indices.size())
		{
			entry.indices.emplace_back(v0);
		}
		else
		{
			entry.indices[entry.last_index] = v0;
		}
		entry.last_index++;
		if (entry.last_index >= entry.indices.size())
		{
			entry.indices.emplace_back(v1);
		}
		else
		{
			entry.indices[entry.last_index] = v1;
		}
		entry.last_index++;
		if (entry.last_index >= entry.indices.size())
		{
			entry.indices.emplace_back(v2);
		}
		else
		{
			entry.indices[entry.last_index] = v2;
		}
	}
}
