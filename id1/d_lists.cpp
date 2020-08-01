#include "quakedef.h"
#include "d_lists.h"
#include "r_shared.h"
#include "r_local.h"
#include "d_local.h"

dlists_t d_lists { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

qboolean d_uselists = false;

extern int r_ambientlight;
extern float r_shadelight;
#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
extern vec3_t r_plightvec;

void D_AddSurfaceToLists (msurface_t* face, surfcache_t* cache, entity_t* entity, qboolean created)
{
	if (face->numedges < 3 || cache->width <= 0 || cache->height <= 0)
	{
		return;
	}
	auto texinfo = face->texinfo;
	d_lists.last_surface++;
	if (d_lists.last_surface >= d_lists.surfaces.size())
	{
		d_lists.surfaces.emplace_back();
	}
	auto& surface = d_lists.surfaces[d_lists.last_surface];
	surface.surface = face;
	surface.entity = entity;
	surface.created = (created ? 1: 0);
	surface.width = cache->width;
	surface.height = cache->height;
	surface.size = surface.width * surface.height;
	if (surface.size > surface.data.size())
	{
		surface.data.resize(surface.size);
	}
	memcpy(surface.data.data(), cache->data, surface.size);
	auto next_front = (d_lists.last_textured_vertex + 1) / 5;
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
	surface.first_index = d_lists.last_textured_index + 1;
	surface.count = (face->numedges - 2) * 3;
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
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v0);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v0;
		}
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v1);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v1;
		}
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v2);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v2;
		}
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
	d_lists.last_turbulent++;
	if (d_lists.last_turbulent >= d_lists.turbulent.size())
	{
		d_lists.turbulent.emplace_back();
	}
	auto& turbulent = d_lists.turbulent[d_lists.last_turbulent];
	turbulent.texture = texture;
	turbulent.width = texture->width;
	turbulent.height = texture->height;
	turbulent.size = turbulent.width * turbulent.height;
	turbulent.data = (unsigned char*)texture + texture->offsets[0];
	auto next_front = (d_lists.last_textured_vertex + 1) / 5;
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
	turbulent.first_index = d_lists.last_textured_index + 1;
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
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v0);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v0;
		}
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v1);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v1;
		}
		d_lists.last_textured_index++;
		if (d_lists.last_textured_index >= d_lists.textured_indices.size())
		{
			d_lists.textured_indices.emplace_back(v2);
		}
		else
		{
			d_lists.textured_indices[d_lists.last_textured_index] = v2;
		}
	}
}

void D_AddAliasToLists (aliashdr_t* aliashdr, trivertx_t* vertices, maliasskindesc_t* skindesc, byte* colormap)
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
	alias.model = mdl;
	alias.width = mdl->skinwidth;
	alias.height = mdl->skinheight;
	alias.size = alias.width * alias.height;
	alias.data = (byte *)aliashdr + skindesc->skin;
	if (colormap == host_colormap)
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
	auto basevert = (d_lists.last_colormapped_vertex + 1) / 6;
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
			d_lists.colormapped_vertices.emplace_back(s);
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
	alias.first_index = d_lists.last_colormapped_index + 1;
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
		d_lists.last_colormapped_index++;
		if (d_lists.last_colormapped_index >= d_lists.colormapped_indices.size())
		{
			d_lists.colormapped_indices.emplace_back(v0);
		}
		else
		{
			d_lists.colormapped_indices[d_lists.last_colormapped_index] = v0;
		}
		d_lists.last_colormapped_index++;
		if (d_lists.last_colormapped_index >= d_lists.colormapped_indices.size())
		{
			d_lists.colormapped_indices.emplace_back(v1);
		}
		else
		{
			d_lists.colormapped_indices[d_lists.last_colormapped_index] = v1;
		}
		d_lists.last_colormapped_index++;
		if (d_lists.last_colormapped_index >= d_lists.colormapped_indices.size())
		{
			d_lists.colormapped_indices.emplace_back(v2);
		}
		else
		{
			d_lists.colormapped_indices[d_lists.last_colormapped_index] = v2;
		}
		triangle++;
	}
}

void D_AddParticleToLists (particle_t* part)
{
	dparticle_t* particle;
	if (d_lists.last_particle >= 0 && d_lists.particles[d_lists.last_particle].color == part->color)
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
		particle->first_index = d_lists.last_colored_index + 1;
		particle->color = part->color;
	}
	auto x = part->org[0] - vright[0] * 0.5 + vup[0] * 0.5;
	auto y = part->org[1] - vright[1] * 0.5 + vup[1] * 0.5;
	auto z = part->org[2] - vright[2] * 0.5 + vup[2] * 0.5;
	auto first_vertex = (d_lists.last_colored_vertex + 1) / 3;
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
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v0);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v0;
	}
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v1);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v1;
	}
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v2);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v2;
	}
	v0 = first_vertex + 2;
	v1 = first_vertex + 3;
	v2 = first_vertex;
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v0);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v0;
	}
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v1);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v1;
	}
	d_lists.last_colored_index++;
	if (d_lists.last_colored_index >= d_lists.colored_indices.size())
	{
		d_lists.colored_indices.emplace_back(v2);
	}
	else
	{
		d_lists.colored_indices[d_lists.last_colored_index] = v2;
	}
}

void D_AddSkyToLists (msurface_t* face, entity_t* entity)
{
	if (d_lists.last_sky >= 0)
	{
		return;
	}
	d_lists.last_sky++;
	if (d_lists.last_sky >= d_lists.sky.size())
	{
		d_lists.sky.emplace_back();
	}
	auto& sky = d_lists.sky[d_lists.last_sky];
	auto first_vertex = (d_lists.last_textured_vertex + 1) / 5;
	float x = -1;
	float y = 1;
	float z = 1;
	float s = 0;
	float t = 0;
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
	x = 1;
	s = 1;
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
	z = -1;
	t = 1;
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
	x = -1;
	s = 0;
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
	sky.first_index = d_lists.last_textured_index + 1;
	sky.count = 6;
	uint32_t v0 = first_vertex;
	uint32_t v1 = first_vertex + 1;
	uint32_t v2 = first_vertex + 2;
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v0);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v0;
	}
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v1);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v1;
	}
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v2);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v2;
	}
	v0 = first_vertex + 2;
	v1 = first_vertex + 3;
	v2 = first_vertex;
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v0);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v0;
	}
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v1);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v1;
	}
	d_lists.last_textured_index++;
	if (d_lists.last_textured_index >= d_lists.textured_indices.size())
	{
		d_lists.textured_indices.emplace_back(v2);
	}
	else
	{
		d_lists.textured_indices[d_lists.last_textured_index] = v2;
	}
}
