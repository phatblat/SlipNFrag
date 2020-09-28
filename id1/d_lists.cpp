#include "quakedef.h"
#include "d_lists.h"
#include "r_shared.h"
#include "r_local.h"
#include "d_local.h"

dlists_t d_lists { -1, -1, -1, -1, -1, -1, -1, -1, -1,-1, -1, -1, -1, -1, -1, -1, -1 };

qboolean d_uselists = false;
qboolean d_awayfromviewmodel = false;

extern int r_ambientlight;
extern float r_shadelight;
#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
extern vec3_t r_plightvec;

void D_AddToIndices16(uint16_t v0, uint16_t v1, uint16_t v2, std::vector<uint16_t>& indices, int& last_index)
{
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v0);
	}
	else
	{
		indices[last_index] = v0;
	}
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v1);
	}
	else
	{
		indices[last_index] = v1;
	}
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v2);
	}
	else
	{
		indices[last_index] = v2;
	}
}

void D_AddToIndices32(uint32_t v0, uint32_t v1, uint32_t v2, std::vector<uint32_t>& indices, int& last_index)
{
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v0);
	}
	else
	{
		indices[last_index] = v0;
	}
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v1);
	}
	else
	{
		indices[last_index] = v1;
	}
	last_index++;
	if (last_index >= indices.size())
	{
		indices.emplace_back(v2);
	}
	else
	{
		indices[last_index] = v2;
	}
}

void D_AddSurfaceToLists (msurface_t* face, surfcache_t* cache, entity_t* entity, qboolean created)
{
	if (face->numedges < 3 || cache->width <= 0 || cache->height <= 0)
	{
		return;
	}
	d_lists.last_surface++;
	if (d_lists.last_surface >= d_lists.surfaces.size())
	{
		d_lists.surfaces.emplace_back();
	}
	auto& surface = d_lists.surfaces[d_lists.last_surface];
	surface.surface = face;
	surface.entity = entity;
	surface.frame_count = r_framecount;
	surface.created = (created ? 1: 0);
	surface.width = cache->width;
	surface.height = cache->height;
	surface.size = surface.width * surface.height;
	if (surface.size > surface.data.size())
	{
		surface.data.resize(surface.size);
	}
	memcpy(surface.data.data(), cache->data, surface.size);
	surface.first_index16 = -1;
	surface.first_index32 = -1;
	surface.first_vertex = (d_lists.last_textured_vertex + 1) / 5;
	surface.count = face->numedges;
	surface.origin_x = entity->origin[0];
	surface.origin_y = entity->origin[1];
	surface.origin_z = entity->origin[2];
	auto new_size = d_lists.last_textured_vertex + 1 + 5 * face->numedges;
	if (d_lists.textured_vertices.size() < new_size)
	{
		d_lists.textured_vertices.resize(new_size);
	}
	auto texinfo = face->texinfo;
	auto edge = entity->model->surfedges[face->firstedge];
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
	s = (s - face->texturemins[0]) / face->extents[0];
	t = (t - face->texturemins[1]) / face->extents[1];
	d_lists.last_textured_vertex++;
	d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
	d_lists.last_textured_vertex++;
	d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
	d_lists.last_textured_vertex++;
	d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
	d_lists.last_textured_vertex++;
	d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
	d_lists.last_textured_vertex++;
	d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
	auto next_front = 0;
	auto next_back = face->numedges;
	auto use_back = false;
	for (auto i = 1; i < face->numedges; i++)
	{
		if (use_back)
		{
			next_back--;
			edge = entity->model->surfedges[face->firstedge + next_back];
		}
		else
		{
			next_front++;
			edge = entity->model->surfedges[face->firstedge + next_front];
		}
		use_back = !use_back;
		if (edge >= 0)
		{
			vertex = &entity->model->vertexes[entity->model->edges[edge].v[0]];
		}
		else
		{
			vertex = &entity->model->vertexes[entity->model->edges[-edge].v[1]];
		}
		x = vertex->position[0];
		y = vertex->position[1];
		z = vertex->position[2];
		s = x * texinfo->vecs[0][0] + y * texinfo->vecs[0][1] + z * texinfo->vecs[0][2] + texinfo->vecs[0][3];
		t = x * texinfo->vecs[1][0] + y * texinfo->vecs[1][1] + z * texinfo->vecs[1][2] + texinfo->vecs[1][3];
		s = (s - face->texturemins[0]) / face->extents[0];
		t = (t - face->texturemins[1]) / face->extents[1];
		d_lists.last_textured_vertex++;
		d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		d_lists.last_textured_vertex++;
		d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		d_lists.last_textured_vertex++;
		d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		d_lists.last_textured_vertex++;
		d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		d_lists.last_textured_vertex++;
		d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
	}
}

void D_AddSpriteToLists (vec5_t* pverts, spritedesc_t* spritedesc)
{
	auto first_vertex = (d_lists.last_textured_vertex + 1) / 5;
	auto is_index16 = (first_vertex + 4 <= 65520);
	auto is_new = true;
	if (d_lists.last_sprite >= 0)
	{
		auto& sprite = d_lists.sprites[d_lists.last_sprite];
		if (sprite.frame == (void*)r_spritedesc.pspriteframe && sprite.frame_count == r_framecount)
		{
			if (sprite.first_index16 >= 0)
			{
				if (is_index16 && sprite.first_index16 + sprite.count == d_lists.last_textured_index16 + 1)
				{
					is_new = false;
				}
			}
			else if (sprite.first_index32 >= 0)
			{
				if (!is_index16 && sprite.first_index32 + sprite.count == d_lists.last_textured_index32 + 1)
				{
					is_new = false;
				}
			}
		}
	}
	if (is_new)
	{
		d_lists.last_sprite++;
		if (d_lists.last_sprite >= d_lists.sprites.size())
		{
			d_lists.sprites.emplace_back();
		}
	}
	auto& sprite = d_lists.sprites[d_lists.last_sprite];
	if (is_new)
	{
		sprite.frame = r_spritedesc.pspriteframe;
		sprite.frame_count = r_framecount;
		sprite.width = spritedesc->pspriteframe->width;
		sprite.height = spritedesc->pspriteframe->height;
		sprite.size = sprite.width * sprite.height;
		if (sprite.size > sprite.data.size())
		{
			sprite.data.resize(sprite.size);
		}
		memcpy(sprite.data.data(), &r_spritedesc.pspriteframe->pixels[0], sprite.size);
		sprite.count = 0;
		if (is_index16)
		{
			sprite.first_index16 = d_lists.last_textured_index16 + 1;
			sprite.first_index32 = -1;
		}
		else
		{
			sprite.first_index16 = -1;
			sprite.first_index32 = d_lists.last_textured_index32 + 1;
		}
	}
	for (auto i = 0; i < 4; i++)
	{
		auto x = pverts[i][0];
		auto y = pverts[i][1];
		auto z = pverts[i][2];
		auto s = pverts[i][3] / sprite.width;
		auto t = pverts[i][4] / sprite.height;
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
	}
	sprite.count += 6;
	auto v0 = first_vertex;
	auto v1 = first_vertex + 1;
	auto v2 = first_vertex + 2;
	if (is_index16)
	{
		D_AddToIndices16(v0, v1, v2, d_lists.textured_indices16, d_lists.last_textured_index16);
	}
	else
	{
		D_AddToIndices32(v0, v1, v2, d_lists.textured_indices32, d_lists.last_textured_index32);
	}
	v0 = first_vertex + 2;
	v1 = first_vertex + 3;
	v2 = first_vertex;
	if (is_index16)
	{
		D_AddToIndices16(v0, v1, v2, d_lists.textured_indices16, d_lists.last_textured_index16);
	}
	else
	{
		D_AddToIndices32(v0, v1, v2, d_lists.textured_indices32, d_lists.last_textured_index32);
	}
}

void D_AddTurbulentToLists (msurface_t* face, entity_t* entity)
{
	if (face->numedges < 3)
	{
		return;
	}
	auto texinfo = face->texinfo;
	auto texture = texinfo->texture;
	auto next_front = (d_lists.last_textured_vertex + 1) / 5;
	auto next_back = next_front + face->numedges - 1;
	auto is_index16 = (next_back < 65520);
	auto is_new = true;
	if (d_lists.last_turbulent >= 0)
	{
		auto& turbulent = d_lists.turbulent[d_lists.last_turbulent];
		if (turbulent.texture == texture && turbulent.frame_count == r_framecount)
		{
			if (turbulent.first_index16 >= 0)
			{
				if (is_index16 && turbulent.first_index16 + turbulent.count == d_lists.last_textured_index16 + 1)
				{
					is_new = false;
				}
			}
			else if (turbulent.first_index32 >= 0)
			{
				if (!is_index16 && turbulent.first_index32 + turbulent.count == d_lists.last_textured_index32 + 1)
				{
					is_new = false;
				}
			}
		}
	}
	if (is_new)
	{
		d_lists.last_turbulent++;
		if (d_lists.last_turbulent >= d_lists.turbulent.size())
		{
			d_lists.turbulent.emplace_back();
		}
	}
	auto& turbulent = d_lists.turbulent[d_lists.last_turbulent];
	if (is_new)
	{
		turbulent.texture = texture;
		turbulent.frame_count = r_framecount;
		turbulent.width = texture->width;
		turbulent.height = texture->height;
		turbulent.size = turbulent.width * turbulent.height;
		turbulent.data = (unsigned char*)texture + texture->offsets[0];
		turbulent.count = 0;
		if (is_index16)
		{
			turbulent.first_index16 = d_lists.last_textured_index16 + 1;
			turbulent.first_index32 = -1;
		}
		else
		{
			turbulent.first_index16 = -1;
			turbulent.first_index32 = d_lists.last_textured_index32 + 1;
		}
	}
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
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
		edgeindex++;
	}
	turbulent.count += (face->numedges - 2) * 3;
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
		if (is_index16)
		{
			D_AddToIndices16(v0, v1, v2, d_lists.textured_indices16, d_lists.last_textured_index16);
		}
		else
		{
			D_AddToIndices32(v0, v1, v2, d_lists.textured_indices32, d_lists.last_textured_index32);
		}
	}
}

void D_AddAliasToLists (aliashdr_t* aliashdr, maliasskindesc_t* skindesc, byte* colormap, trivertx_t* vertices)
{
	auto mdl = (mdl_t *)((byte *)aliashdr + aliashdr->model);
	if (mdl->numtris <= 0)
	{
		return;
	}
	auto basevert = (d_lists.last_colormapped_vertex + 1) / 6;
	auto skin_start = (byte *)aliashdr + skindesc->skin;
	auto is_index16 = (basevert + mdl->numverts * 2 <= 65520);
	auto is_new = true;
	if (d_lists.last_alias >= 0)
	{
		auto& alias = d_lists.alias[d_lists.last_alias];
		if (alias.data == skin_start && alias.frame_count == r_framecount)
		{
			if (alias.first_index16 >= 0)
			{
				if (is_index16 && alias.first_index16 + alias.count == d_lists.last_colormapped_index16 + 1)
				{
					is_new = false;
				}
			}
			else if (alias.first_index32 >= 0)
			{
				if (!is_index16 && alias.first_index32 + alias.count == d_lists.last_colormapped_index32 + 1)
				{
					is_new = false;
				}
			}
		}
	}
	if (is_new)
	{
		d_lists.last_alias++;
		if (d_lists.last_alias >= d_lists.alias.size())
		{
			d_lists.alias.emplace_back();
		}
	}
	auto& alias = d_lists.alias[d_lists.last_alias];
	if (is_new)
	{
		alias.frame_count = r_framecount;
		alias.width = mdl->skinwidth;
		alias.height = mdl->skinheight;
		alias.size = alias.width * alias.height;
		alias.data = skin_start;
		if (colormap == host_colormap.data())
		{
			alias.is_host_colormap = true;
		}
		else
		{
			alias.is_host_colormap = false;
			if (alias.colormap.size() < 16384)
			{
				alias.colormap.resize(16384);
			}
			memcpy(alias.colormap.data(), colormap, 16384);
		}
		alias.count = 0;
		if (is_index16)
		{
			alias.first_index16 = d_lists.last_colormapped_index16 + 1;
			alias.first_index32 = -1;
		}
		else
		{
			alias.first_index16 = -1;
			alias.first_index32 = d_lists.last_colormapped_index32 + 1;
		}
	}
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
	for (auto i = 0; i < mdl->numverts; i++)
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

		// lighting
		auto plightnormal = r_avertexnormals[vertex->lightnormalindex];
		auto lightcos = DotProduct (plightnormal, r_plightvec);
		auto temp = r_ambientlight;

		if (lightcos < 0)
		{
			temp += (int)(r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading light, we
			// don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		float light = temp / 256;

		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(x);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = x;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(z);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = z;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = -y;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(s);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = s;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(t);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = t;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(light);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = light;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(x);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = x;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(z);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = z;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = -y;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(s + 0.5);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = s + 0.5;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(t);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = t;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(light);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = light;
		}
		vertex++;
		texcoords++;
	}
	alias.count += mdl->numtris * 3;
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
		if (is_index16)
		{
			D_AddToIndices16(v0, v1, v2, d_lists.colormapped_indices16, d_lists.last_colormapped_index16);
		}
		else
		{
			D_AddToIndices32(v0, v1, v2, d_lists.colormapped_indices32, d_lists.last_colormapped_index32);
		}
		triangle++;
	}
}

void D_AddViewModelToLists (aliashdr_t* aliashdr, maliasskindesc_t* skindesc, byte* colormap, trivertx_t* vertices)
{
	auto mdl = (mdl_t *)((byte *)aliashdr + aliashdr->model);
	if (mdl->numtris <= 0)
	{
		return;
	}
	d_lists.last_viewmodel++;
	if (d_lists.last_viewmodel >= d_lists.viewmodel.size())
	{
		d_lists.viewmodel.emplace_back();
	}
	auto& view_model = d_lists.viewmodel[d_lists.last_viewmodel];
	view_model.frame_count = r_framecount;
	view_model.width = mdl->skinwidth;
	view_model.height = mdl->skinheight;
	view_model.size = view_model.width * view_model.height;
	view_model.data = (byte *)aliashdr + skindesc->skin;
	if (colormap == host_colormap.data())
	{
		view_model.is_host_colormap = true;
	}
	else
	{
		view_model.is_host_colormap = false;
		if (view_model.colormap.size() < 16384)
		{
			view_model.colormap.resize(16384);
		}
		memcpy(view_model.colormap.data(), colormap, 16384);
	}
	vec3_t angles;
	if (d_awayfromviewmodel)
	{
		angles[ROLL] = 0;
		angles[PITCH] = 0;
		angles[YAW] = 0;
	}
	else
	{
		angles[ROLL] = currententity->angles[ROLL];
		angles[PITCH] = -currententity->angles[PITCH];
		angles[YAW] = currententity->angles[YAW];
	}
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
	if (d_awayfromviewmodel)
	{
		t2matrix[0][3] -= forward[0] * 8;
		t2matrix[1][3] -= forward[1] * 8;
		t2matrix[2][3] -= forward[2] * 8;
	}
	float transform[3][4];
	R_ConcatTransforms (t2matrix, tmatrix, transform);
	auto vertex = vertices;
	auto texcoordsbase = (stvert_t *)((byte *)aliashdr + aliashdr->stverts);
	auto texcoords = texcoordsbase;
	auto basevert = (d_lists.last_colormapped_vertex + 1) / 6;
	for (auto i = 0; i < mdl->numverts; i++)
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
		s /= view_model.width;
		t /= view_model.height;

		// lighting
		float light;
		if (d_awayfromviewmodel)
		{
			light = 0;
		}
		else
		{
			auto plightnormal = r_avertexnormals[vertex->lightnormalindex];
			auto lightcos = DotProduct (plightnormal, r_plightvec);
			auto temp = r_ambientlight;

			if (lightcos < 0)
			{
				temp += (int)(r_shadelight * lightcos);

				// clamp; because we limited the minimum ambient and shading light, we
				// don't have to clamp low light, just bright
				if (temp < 0)
					temp = 0;
			}

			light = temp / 256;
		}

		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(x);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = x;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(z);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = z;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = -y;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(s);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = s;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(t);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = t;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(light);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = light;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(x);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = x;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(z);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = z;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = -y;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(s + 0.5);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = s + 0.5;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(t);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = t;
		}
		d_lists.last_colormapped_vertex++;
		if (d_lists.last_colormapped_vertex >= d_lists.colormapped_vertices.size())
		{
			d_lists.colormapped_vertices.emplace_back(light);
		}
		else
		{
			d_lists.colormapped_vertices[d_lists.last_colormapped_vertex] = light;
		}
		vertex++;
		texcoords++;
	}
	auto is_index16 = (basevert + mdl->numverts * 2 <= 65520);
	if (is_index16)
	{
		view_model.first_index16 = d_lists.last_colormapped_index16 + 1;
		view_model.first_index32 = -1;
	}
	else
	{
		view_model.first_index16 = -1;
		view_model.first_index32 = d_lists.last_colormapped_index32 + 1;
	}
	view_model.count = mdl->numtris * 3;
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
		if (is_index16)
		{
			D_AddToIndices16(v0, v1, v2, d_lists.colormapped_indices16, d_lists.last_colormapped_index16);
		}
		else
		{
			D_AddToIndices32(v0, v1, v2, d_lists.colormapped_indices32, d_lists.last_colormapped_index32);
		}
		triangle++;
	}
}

void D_AddParticleToLists (particle_t* part)
{
	auto first_vertex = (d_lists.last_colored_vertex + 1) / 3;
	auto is_index16 = (first_vertex + 4 <= 65520);
	dparticle_t* particle;
	if (d_lists.last_particle >= 0 && d_lists.particles[d_lists.last_particle].color == part->color && ((is_index16 && (d_lists.particles[d_lists.last_particle].first_index16 >= 0)) || (!is_index16 && (d_lists.particles[d_lists.last_particle].first_index32 >= 0))))
	{
		particle = d_lists.particles.data() + d_lists.last_particle;
	}
	else
	{
		d_lists.last_particle++;
		if (d_lists.last_particle >= d_lists.particles.size())
		{
			d_lists.particles.emplace_back();
			particle = d_lists.particles.data() + d_lists.last_particle;
		}
		else
		{
			particle = d_lists.particles.data() + d_lists.last_particle;
			particle->count = 0;
		}
		if (is_index16)
		{
			particle->first_index16 = d_lists.last_colored_index16 + 1;
			particle->first_index32 = -1;
		}
		else
		{
			particle->first_index16 = -1;
			particle->first_index32 = d_lists.last_colored_index32 + 1;
		}
		particle->color = part->color;
	}
	auto x = part->org[0] - vright[0] * 0.5 + vup[0] * 0.5;
	auto y = part->org[1] - vright[1] * 0.5 + vup[1] * 0.5;
	auto z = part->org[2] - vright[2] * 0.5 + vup[2] * 0.5;
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(x);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = x;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(z);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = z;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(-y);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = -y;
	}
	x += vright[0];
	y += vright[1];
	z += vright[2];
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(x);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = x;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(z);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = z;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(-y);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = -y;
	}
	x -= vup[0];
	y -= vup[1];
	z -= vup[2];
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(x);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = x;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(z);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = z;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(-y);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = -y;
	}
	x -= vright[0];
	y -= vright[1];
	z -= vright[2];
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(x);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = x;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(z);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = z;
	}
	d_lists.last_colored_vertex++;
	if (d_lists.last_colored_vertex >= d_lists.colored_vertices.size())
	{
		d_lists.colored_vertices.emplace_back(-y);
	}
	else
	{
		d_lists.colored_vertices[d_lists.last_colored_vertex] = -y;
	}
	particle->count += 6;
	auto v0 = first_vertex;
	auto v1 = first_vertex + 1;
	auto v2 = first_vertex + 2;
	if (is_index16)
	{
		D_AddToIndices16(v0, v1, v2, d_lists.colored_indices16, d_lists.last_colored_index16);
	}
	else
	{
		D_AddToIndices32(v0, v1, v2, d_lists.colored_indices32, d_lists.last_colored_index32);
	}
	v0 = first_vertex + 2;
	v1 = first_vertex + 3;
	v2 = first_vertex;
	if (is_index16)
	{
		D_AddToIndices16(v0, v1, v2, d_lists.colored_indices16, d_lists.last_colored_index16);
	}
	else
	{
		D_AddToIndices32(v0, v1, v2, d_lists.colored_indices32, d_lists.last_colored_index32);
	}
}

void D_AddSkyToLists (surf_t* surf, msurface_t* face, entity_t* entity)
{
	auto pspan = surf->spans;
	if (pspan == nullptr)
	{
		return;
	}
	int left;
	int right;
	int top;
	int bottom;
	while (pspan != nullptr)
	{
		if (pspan == surf->spans)
		{
			left = pspan->u;
			right = left + pspan->count;
			top = pspan->v;
			bottom = pspan->v;
		}
		else
		{
			left = std::min(left, pspan->u);
			right = std::max(right, left + pspan->count);
			top = std::min(top, pspan->v);
			bottom = std::max(bottom, pspan->v);
		}
		pspan = pspan->pnext;
	}
	left -= vid.width / 10;
	right += vid.height / 10;
	top -= vid.height / 10;
	bottom += vid.height / 10;
	qboolean added = false;
	if (d_lists.last_sky < 0)
	{
		d_lists.last_sky++;
		if (d_lists.last_sky >= d_lists.sky.size())
		{
			d_lists.sky.emplace_back();
		}
		added = true;
	}
	auto& sky = d_lists.sky[d_lists.last_sky];
	if (added)
	{
		sky.left = (float)left / (float)vid.width;
		sky.right = (float)right / (float)vid.width;
		sky.top = (float)top / (float)vid.height;
		sky.bottom = (float)bottom / (float)vid.height;
	}
	else
	{
		sky.left = std::min(sky.left, (float)left / (float)vid.width);
		sky.right = std::max(sky.right, (float)right / (float)vid.width);
		sky.top = std::min(sky.top, (float)top / (float)vid.height);
		sky.bottom = std::max(sky.bottom, (float)bottom / (float)vid.height);
	}
	if (added)
	{
		auto first_vertex = (d_lists.last_textured_vertex + 1) / 5;
		float x = sky.left * 2 - 1;
		float y = 1;
		float z = 1 - sky.top * 2;
		float s = sky.left;
		float t = sky.top;
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
		x = sky.right * 2 - 1;
		s = sky.right;
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
		z = 1 - sky.bottom * 2;
		t = sky.bottom;
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
		x = sky.left * 2 - 1;
		s = sky.left;
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(x);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = x;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(z);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = z;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(-y);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = -y;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(s);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = s;
		}
		d_lists.last_textured_vertex++;
		if (d_lists.last_textured_vertex >= d_lists.textured_vertices.size())
		{
			d_lists.textured_vertices.emplace_back(t);
		}
		else
		{
			d_lists.textured_vertices[d_lists.last_textured_vertex] = t;
		}
		auto is_index16 = (first_vertex + 4 <= 65520);
		if (is_index16)
		{
			sky.first_index16 = d_lists.last_textured_index16 + 1;
			sky.first_index32 = -1;
		}
		else
		{
			sky.first_index16 = -1;
			sky.first_index32 = d_lists.last_textured_index32 + 1;
		}
		sky.count = 6;
		uint32_t v0 = first_vertex;
		uint32_t v1 = first_vertex + 1;
		uint32_t v2 = first_vertex + 2;
		if (is_index16)
		{
			D_AddToIndices16(v0, v1, v2, d_lists.textured_indices16, d_lists.last_textured_index16);
		}
		else
		{
			D_AddToIndices32(v0, v1, v2, d_lists.textured_indices32, d_lists.last_textured_index32);
		}
		v0 = first_vertex + 2;
		v1 = first_vertex + 3;
		v2 = first_vertex;
		if (is_index16)
		{
			D_AddToIndices16(v0, v1, v2, d_lists.textured_indices16, d_lists.last_textured_index16);
		}
		else
		{
			D_AddToIndices32(v0, v1, v2, d_lists.textured_indices32, d_lists.last_textured_index32);
		}
	}
	else
	{
		int position;
		if (sky.first_index16 >= 0)
		{
			position = d_lists.textured_indices16[sky.first_index16];
		}
		else
		{
			position = d_lists.textured_indices32[sky.first_index32];
		}
		position *= 5;
		float x = sky.left * 2 - 1;
		float y = 1;
		float z = 1 - sky.top * 2;
		float s = sky.left;
		float t = sky.top;
		d_lists.textured_vertices[position] = x;
		position++;
		d_lists.textured_vertices[position] = z;
		position++;
		d_lists.textured_vertices[position] = -y;
		position++;
		d_lists.textured_vertices[position] = s;
		position++;
		d_lists.textured_vertices[position] = t;
		position++;
		x = sky.right * 2 - 1;
		s = sky.right;
		d_lists.textured_vertices[position] = x;
		position++;
		d_lists.textured_vertices[position] = z;
		position++;
		d_lists.textured_vertices[position] = -y;
		position++;
		d_lists.textured_vertices[position] = s;
		position++;
		d_lists.textured_vertices[position] = t;
		position++;
		z = 1 - sky.bottom * 2;
		t = sky.bottom;
		d_lists.textured_vertices[position] = x;
		position++;
		d_lists.textured_vertices[position] = z;
		position++;
		d_lists.textured_vertices[position] = -y;
		position++;
		d_lists.textured_vertices[position] = s;
		position++;
		d_lists.textured_vertices[position] = t;
		position++;
		x = sky.left * 2 - 1;
		s = sky.left;
		d_lists.textured_vertices[position] = x;
		position++;
		d_lists.textured_vertices[position] = z;
		position++;
		d_lists.textured_vertices[position] = -y;
		position++;
		d_lists.textured_vertices[position] = s;
		position++;
		d_lists.textured_vertices[position] = t;
	}
}
