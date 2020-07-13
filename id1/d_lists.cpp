#include "quakedef.h"
#include "d_lists.h"
#include "r_shared.h"
#include "r_local.h"
#include "d_local.h"

dlists_t d_lists { -1, -1, -1, -1, -1, -1 };

qboolean d_uselists = false;

void D_AddTurbulentToLists (msurface_t* face, entity_t* entity)
{
	if (face->numedges < 3)
	{
		return;
	}
	auto texinfo = face->texinfo;
	auto texture = texinfo->texture;
	d_lists.last_turbulent++;
	if (d_lists.last_turbulent >= d_lists.turbulent.size())
	{
		d_lists.turbulent.emplace_back();
	}
	auto& turbulent = d_lists.turbulent[d_lists.last_turbulent];
	turbulent.width = texture->width;
	turbulent.height = texture->height;
	turbulent.size = turbulent.width * turbulent.height;
	if (turbulent.size > turbulent.data.size())
	{
		turbulent.data.resize(turbulent.size);
	}
	memcpy(turbulent.data.data(), (byte*)texture + texture->offsets[0], turbulent.size);
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
		s /= turbulent.width;
		t /= turbulent.height;
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
	turbulent.first_index = d_lists.last_index + 1;
	turbulent.count = (face->numedges - 2) * 3;
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

void D_AddTexturedToLists (msurface_t* face, surfcache_t* cache, entity_t* entity)
{
	if (face->numedges < 3 || cache->width <= 0 || cache->height <= 0 || d_lists.textured_index.find((void*)face) != d_lists.textured_index.end())
	{
		return;
	}
	d_lists.textured_index.insert((void*)face);
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

void D_AddAliasToLists (aliashdr_t* aliashdr, trivertx_t* vertices, maliasskindesc_t* skindesc)
{
	auto mdl = (mdl_t *)((byte *)aliashdr + aliashdr->model);
	if (mdl->numtris <= 0)
	{
		return;
	}
	d_lists.last_alias++;
	if (d_lists.last_alias >= d_lists.alias.size())
	{
		d_lists.alias.emplace_back();
	}
	auto& alias = d_lists.alias[d_lists.last_alias];
	alias.width = mdl->skinwidth;
	alias.height = mdl->skinheight;
	alias.size = alias.width * alias.height;
	if (alias.size > alias.data.size())
	{
		alias.data.resize(alias.size);
	}
	memcpy(alias.data.data(), (byte *)aliashdr + skindesc->skin, alias.size);
	vec3_t angles;
	angles[ROLL] = currententity->angles[ROLL];
	angles[PITCH] = -currententity->angles[PITCH];
	angles[YAW] = currententity->angles[YAW];
	vec3_t forward, right, up;
	AngleVectors (angles, forward, right, up);
	float tmatrix[3][4] { };
	tmatrix[0][0] = mdl->scale[0];
	tmatrix[1][1] = mdl->scale[1];
	tmatrix[2][2] = mdl->scale[2];
	tmatrix[0][3] = mdl->scale_origin[0];
	tmatrix[1][3] = mdl->scale_origin[1];
	tmatrix[2][3] = mdl->scale_origin[2];
	float t2matrix[3][4] { };
	for (auto i = 0; i < 3; i++)
	{
		t2matrix[i][0] = forward[i];
		t2matrix[i][1] = -right[i];
		t2matrix[i][2] = up[i];
	}
	t2matrix[0][3] = currententity->origin[0];
	t2matrix[1][3] = currententity->origin[1];
	t2matrix[2][3] = currententity->origin[2];
	float transform[3][4];
	R_ConcatTransforms (t2matrix, tmatrix, transform);
	auto vertex = vertices;
	auto texcoordsbase = (stvert_t *)((byte *)aliashdr + aliashdr->stverts);
	auto texcoords = texcoordsbase;
	auto basevert = (d_lists.last_vertex + 1) / 5;
	for (auto i = 0; i < mdl->numverts;  i++)
	{
		vec3_t pos { (float)vertex->v[0], (float)vertex->v[1], (float)vertex->v[2] };
		vec3_t trans;
		trans[0] = DotProduct(pos, transform[0]) + transform[0][3];
		trans[1] = DotProduct(pos, transform[1]) + transform[1][3];
		trans[2] = DotProduct(pos, transform[2]) + transform[2][3];
		auto x = trans[0];
		auto y = trans[1];
		auto z = trans[2];
		auto s = (float)(texcoords->s >> 16);
		auto t = (float)(texcoords->t >> 16);
		s /= alias.width;
		t /= alias.height;
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
			d_lists.vertices[d_lists.last_vertex] = s + 0.5;
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
		vertex++;
		texcoords++;
	}
	alias.first_index = d_lists.last_index + 1;
	alias.count = mdl->numtris * 3;
	auto triangle = (mtriangle_t *)((byte *)aliashdr + aliashdr->triangles);
	for (auto i = 0; i < mdl->numtris; i++)
	{
		auto v0 = triangle->vertindex[0];
		auto v1 = triangle->vertindex[1];
		auto v2 = triangle->vertindex[2];
		auto v0back = (((texcoordsbase[v0].onseam & ALIAS_ONSEAM) == ALIAS_ONSEAM) && triangle->facesfront == 0);
		auto v1back = (((texcoordsbase[v1].onseam & ALIAS_ONSEAM) == ALIAS_ONSEAM) && triangle->facesfront == 0);
		auto v2back = (((texcoordsbase[v2].onseam & ALIAS_ONSEAM) == ALIAS_ONSEAM) && triangle->facesfront == 0);
		v0 = basevert + v0 * 2 + (v0back ? 1 : 0);
		v1 = basevert + v1 * 2 + (v1back ? 1 : 0);
		v2 = basevert + v2 * 2 + (v2back ? 1 : 0);
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
		triangle++;
	}
}
