#include "quakedef.h"
#include "r_lists.h"

lists_t r_lists { -1 };

qboolean r_uselists = false;

void R_AddFaceToLists (msurface_t* face)
{
	if (face->numedges < 3)
	{
		return;
	}
	auto texinfo = face->texinfo;
	auto texture = texinfo->texture;
	if (r_lists.last_textured < 0 || (r_lists.textured[r_lists.last_textured].key != (void*)texture))
	{
		r_lists.last_textured++;
		if (r_lists.last_textured >= r_lists.textured.size())
		{
			r_lists.textured.emplace_back();
		}
		auto& entry = r_lists.textured[r_lists.last_textured];
		entry.key = (void*)texture;
		entry.width = texture->width;
		entry.height = texture->height;
		auto size = entry.width * entry.height;
		auto fullSize = size * 85 / 64;
		entry.size = fullSize;
		if (fullSize > entry.data.size())
		{
			entry.data.resize(fullSize);
		}
		memcpy(entry.data.data(), ((byte *)texture + texture->offsets[0]), size);
		auto offset = size;
		size /= 4;
		memcpy(entry.data.data() + offset, ((byte *)texture + texture->offsets[1]), size);
		offset += size;
		size /= 4;
		memcpy(entry.data.data() + offset, ((byte *)texture + texture->offsets[2]), size);
		offset += size;
		size /= 4;
		memcpy(entry.data.data() + offset, ((byte *)texture + texture->offsets[3]), size);
		entry.last_vertex = -1;
		entry.last_index = -1;
	}
	auto& entry = r_lists.textured[r_lists.last_textured];
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
		auto s = vertex->position[0] * texinfo->vecs[0][0] + vertex->position[1] * texinfo->vecs[0][1] + vertex->position[2] * texinfo->vecs[0][2] + texinfo->vecs[0][3];
		auto t = vertex->position[0] * texinfo->vecs[1][0] + vertex->position[1] * texinfo->vecs[1][1] + vertex->position[2] * texinfo->vecs[1][2] + texinfo->vecs[1][3];
		entry.last_vertex++;
		if (entry.last_vertex >= entry.vertices.size())
		{
			entry.vertices.emplace_back();
			entry.texcoords.emplace_back();
		}
		auto& vert = entry.vertices[entry.last_vertex];
		vert.x = -vertex->position[0];
		vert.y = vertex->position[2];
		vert.z = vertex->position[1];
		auto& tex = entry.texcoords[entry.last_vertex];
		tex.s = s / texture->width;
		tex.t = t / texture->height;
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
