#include "quakedef.h"
#include "d_lists.h"
#include "d_local.h"

dlists_t d_lists { -1, -1, -1 };

qboolean d_uselists = false;

void D_AddFaceToLists (msurface_t* face, surfcache_t* cache, entity_t* entity)
{
	if (face->numedges < 3 || cache->width <= 0 || cache->height <= 0)
	{
		return;
	}
	auto texinfo = face->texinfo;
	d_lists.last_textured++;
	if (d_lists.last_textured >= d_lists.textured.size())
	{
		d_lists.textured.emplace_back();
	}
	auto& textured = d_lists.textured[d_lists.last_textured];
	textured.width = cache->width;
	textured.height = cache->height;
	textured.size = textured.width * textured.height;
	if (textured.size > textured.data.size())
	{
		textured.data.resize(textured.size);
	}
	memcpy(textured.data.data(), cache->data, textured.size);
	auto next_front = (d_lists.last_vertex + 1) / 5;
	auto next_back = next_front + face->numedges - 1;
	auto edgeindex = face->firstedge;
	for (auto i = 0; i < face->numedges; i++)
	{
		auto edge = entity->model->surfedges[edgeindex];
		mvertex_t* vertex;
		if (edge >= 0)
		{
			vertex = &entity->model->vertexes[entity->model->edges[edge].v[0]];
		}
		else
		{
			vertex = &entity->model->vertexes[entity->model->edges[-edge].v[1]];
		}
		auto x = vertex->position[0];
		auto y = vertex->position[1];
		auto z = vertex->position[2];
		auto s = x * texinfo->vecs[0][0] + y * texinfo->vecs[0][1] + z * texinfo->vecs[0][2] + texinfo->vecs[0][3];
		auto t = x * texinfo->vecs[1][0] + y * texinfo->vecs[1][1] + z * texinfo->vecs[1][2] + texinfo->vecs[1][3];
		x += entity->origin[0];
		y += entity->origin[1];
		z += entity->origin[2];
		s = (s - face->texturemins[0]) / face->extents[0];
		t = (t - face->texturemins[1]) / face->extents[1];
		d_lists.last_vertex++;
		if (d_lists.last_vertex >= d_lists.vertices.size())
		{
			d_lists.vertices.emplace_back(x);
		}
		else
		{
			d_lists.vertices[d_lists.last_vertex] = x;
		}
		d_lists.last_vertex++;
		if (d_lists.last_vertex >= d_lists.vertices.size())
		{
			d_lists.vertices.emplace_back(z);
		}
		else
		{
			d_lists.vertices[d_lists.last_vertex] = z;
		}
		d_lists.last_vertex++;
		if (d_lists.last_vertex >= d_lists.vertices.size())
		{
			d_lists.vertices.emplace_back(-y);
		}
		else
		{
			d_lists.vertices[d_lists.last_vertex] = -y;
		}
		d_lists.last_vertex++;
		if (d_lists.last_vertex >= d_lists.vertices.size())
		{
			d_lists.vertices.emplace_back(s);
		}
		else
		{
			d_lists.vertices[d_lists.last_vertex] = s;
		}
		d_lists.last_vertex++;
		if (d_lists.last_vertex >= d_lists.vertices.size())
		{
			d_lists.vertices.emplace_back(t);
		}
		else
		{
			d_lists.vertices[d_lists.last_vertex] = t;
		}
		edgeindex++;
	}
	textured.first_index = d_lists.last_index + 1;
	textured.count = (face->numedges - 2) * 3;
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
		d_lists.last_index++;
		if (d_lists.last_index >= d_lists.indices.size())
		{
			d_lists.indices.emplace_back(v0);
		}
		else
		{
			d_lists.indices[d_lists.last_index] = v0;
		}
		d_lists.last_index++;
		if (d_lists.last_index >= d_lists.indices.size())
		{
			d_lists.indices.emplace_back(v1);
		}
		else
		{
			d_lists.indices[d_lists.last_index] = v1;
		}
		d_lists.last_index++;
		if (d_lists.last_index >= d_lists.indices.size())
		{
			d_lists.indices.emplace_back(v2);
		}
		else
		{
			d_lists.indices[d_lists.last_index] = v2;
		}
	}
}
