#include "PXL_Batch.h"
#include <iostream>
#include <algorithm>
#include "PXL_Window.h"
#include "system/PXL_Exception.h"

PXL_Batch::PXL_Batch(PXL_BatchSize max_vertices) {
	vbo_created = false;
	create_batch(max_vertices);
}

PXL_Batch* PXL_create_batch(PXL_BatchSize max_vertices) {
	return new PXL_Batch(max_vertices);
}

void PXL_Batch::create_batch(PXL_BatchSize max_vertices) {
	max_vertices_amount = max_vertices;
	max_quads_amount = max_vertices_amount / 4;

	//if the batch is already created then delete the vbo
	if (vbo_created) { free(); }

	{
		//create the vbo
		glGenBuffers(1, &vertex_buffer_id);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
		glBufferData(GL_ARRAY_BUFFER, max_vertices_amount * sizeof(PXL_VertexPoint), NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, NULL);

		current_texture_ids = new SparseTextureId[max_quads_amount];
		next_texture_ids = new SparseTextureId[max_quads_amount];
		vertex_batches = new PXL_VertexBatch[max_quads_amount];
		vertex_data = new PXL_VertexPoint[max_vertices_amount];
		vbo_created = true;
	}

	clear_all();

	//set perspective matrix to window coordinates and translate to 0,0 top left
	view_mat.identity();
	perspective_mat.identity();

	perspective_mat.scale(1.0f / PXL_center_window_x, -1.0f / PXL_center_window_y);
	perspective_mat.translate(-1.0f, 1.0f);

	//enable alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//binds vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);

	//enable vertex attrib pointers when rendering
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	//set vertex shader attrib pointers
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PXL_VertexPoint), (void*)offsetof(PXL_VertexPoint, pos));
	glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(PXL_VertexPoint), (void*)offsetof(PXL_VertexPoint, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PXL_VertexPoint), (void*)offsetof(PXL_VertexPoint, colour));
}

void PXL_Batch::render_all() {
	if (num_added != 0) {
		//if a framebuffer is specified, bind to it, if not bind to the default framebuffer
		if (target_frame_buffer != NULL) {
			target_frame_buffer->bind(PXL_GL_FRAMEBUFFER_WRITE);
		}else {
			glBindFramebuffer(PXL_GL_FRAMEBUFFER_WRITE, 0);
		}

		//clear depth buffer bit
		glClear(GL_DEPTH_BUFFER_BIT);

		draw_vbo();
	}
	clear_all();
}

void PXL_Batch::clear_all() {
	last_freq_index = 0;
	for (int n = min_texture_id; n <= max_texture_id; ++n) {
		if (current_texture_ids[n].frequency >= 1) {
			next_texture_ids[n].frequency = current_texture_ids[n].frequency;
			next_texture_ids[n].batch_index = last_freq_index;
			last_freq_index += current_texture_ids[n].frequency;
			current_texture_ids[n].frequency = 0;
			current_texture_ids[n].batch_index = 0;
		}
	}

	num_added = 0;
	vertex_batch_index = 0;
	min_texture_id = UINT_MAX;
	max_texture_id = 0;
}

void PXL_Batch::use_shader(PXL_ShaderProgram* shader) {
	if (shader == NULL) { shader = PXL_default_shader; }

	if (current_shader != shader) {
		current_shader = shader;

		//use specified program id
		glUseProgram(current_shader->get_program_id());

		//set matrix uniform in the vertex shader for the program
		view_mat.identity();
		glUniformMatrix4fv(current_shader->get_matrix_loc(), 1, true, (view_mat * perspective_mat).get_mat());
	}
}

void PXL_Batch::use_blend_mode(PXL_BlendMode blend_mode) {
	if (current_blend_mode != blend_mode) {
		current_blend_mode = blend_mode;
		if (current_blend_mode == PXL_ALPHA_BLEND) {
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);
		}else if (current_blend_mode == PXL_ALPHA_NO_BLEND) {
			glEnable(GL_DEPTH_TEST);
			glAlphaFunc(GL_GREATER, .01f);
			glEnable(GL_ALPHA_TEST);
		}else if (current_blend_mode == PXL_ALPHA_NONE) {
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);
		}
	}
}

void PXL_Batch::add(PXL_Texture* texture, PXL_Rect* rect, PXL_Rect* src_rect, float rotation, PXL_Vec2* origin, 
					PXL_Flip flip, float r, float g, float b, float a, PXL_ShaderProgram* shader, PXL_BlendMode blend_mode, float z_depth) {
	if (verify_texture_add(texture, rect)) {
		add_quad(texture, rect, src_rect, rotation, origin, flip, r, g, b, a, shader, blend_mode, z_depth);
		++num_added;
	}
}

bool PXL_Batch::verify_texture_add(PXL_Texture* texture, PXL_Rect* rect) {
	if (texture->texture_created) {
		if (rect->x + rect->w > 0 && rect->y + rect->h > 0 && rect->x < PXL_window_width && rect->y < PXL_window_height) {
			if (num_added + 1 >= max_quads_amount) {
				PXL_show_exception("Hit max batch size at " + std::to_string(max_quads_amount) + " max sprites/quads");
				return false;
			}

			return true;
		}
	}
	return false;
}

void PXL_Batch::add_quad(PXL_Texture* texture, PXL_Rect* rect, PXL_Rect* src_rect, 
						 float rotation, PXL_Vec2* origin, PXL_Flip flip, 
						 float r, float g, float b, float a, PXL_ShaderProgram* shader, PXL_BlendMode blend_mode, float z_depth) {
	//set the texture id and shader program for the vertex batch
	int index = last_freq_index;
	GLuint texture_id = texture->get_gl_id();
	if (next_texture_ids[texture_id].frequency >= 1) {
		index = next_texture_ids[texture_id].batch_index;
		++next_texture_ids[texture_id].batch_index;
		if (next_texture_ids[texture_id].batch_index >= max_quads_amount) { next_texture_ids[texture_id].batch_index = 0; index = 0; }
	}else {
		++last_freq_index;
		if (last_freq_index >= max_quads_amount) { last_freq_index = 0; index = 0; }
	}
	v_batch = vertex_batches + index;
	v_batch->num_vertices = 4;
	v_batch->texture_id = texture_id;
	v_batch->shader = shader;
	v_batch->z_depth = z_depth;

	if (blend_mode == PXL_ALPHA_AUTO_NO_BLEND || blend_mode == PXL_ALPHA_AUTO_BLEND) {
		if (texture->has_transparency) {
			if (blend_mode == PXL_ALPHA_AUTO_NO_BLEND) {
				blend_mode = PXL_ALPHA_NO_BLEND;
			}else if (blend_mode == PXL_ALPHA_AUTO_BLEND) {
				blend_mode = PXL_ALPHA_BLEND;
			}
		}else {
			if (a != 1 && blend_mode == PXL_ALPHA_AUTO_BLEND) {
				blend_mode = PXL_ALPHA_BLEND;
			}else {
				blend_mode = PXL_ALPHA_NONE;
			}
		}
	}
	v_batch->blend_mode = blend_mode;

	current_texture_ids[texture_id].texture = texture;
	++current_texture_ids[texture_id].frequency;
	min_texture_id = PXL_min(min_texture_id, texture_id);
	max_texture_id = PXL_max(max_texture_id, texture_id);

	//set vertex pos, uvs and colours
	PXL_VertexPoint* v = vertex_data + (index * 4);

	v[0].z_depth = z_depth;
	v[1].z_depth = z_depth;
	v[2].z_depth = z_depth;
	v[3].z_depth = z_depth;

	/**
	==================================================================================
								Set vertex positions
	==================================================================================
	**/
	//set origin
	float origin_x = 0; float origin_y = 0;
	if (origin != NULL) { origin_x = origin->x; origin_y = origin->y; }

	bool rect_changed = false;
	if (v_batch->rect.x != rect->x || v_batch->rect.y != rect->y ||
		v_batch->rect.w != rect->w || v_batch->rect.h != rect->h) {
		rect_changed = true;
	}
	bool rotate_changed = false;
	if (v_batch->rotation != rotation || v_batch->flip != flip ||
		v_batch->origin.x != origin_x || v_batch->origin.y != origin_y) {
		rotate_changed = true;
	}
	if (rect_changed || rotate_changed || v_batch->flip != flip) {
		//get positions from rect
		int x = rect->x; int y = rect->y;

		//set scale
		float scale_x = rect->w / texture->get_width(); float scale_y = rect->h / texture->get_height();
		if (v_batch->rect.x != rect->x || v_batch->rect.y != rect->y ||
			v_batch->rect.w != rect->w || v_batch->rect.h != rect->h) {
			if (flip == PXL_FLIP_HORIZONTAL) {
				scale_x = -scale_x;
				x += rect->w;
				origin_x -= rect->w;
			}else if (flip == PXL_FLIP_VERTICAL) {
				scale_y = -scale_y;
				y += rect->h;
				origin_y -= rect->h;
			}
		}
		int scaled_width = texture->get_width() * scale_x;
		int scaled_height = texture->get_height() * scale_y;

		//apply rotation
		if (rotation != 0) {
			//set rotation to degrees rather than radians
			rotation = rotation / PXL_radian;
			float c = PXL_fast_cos(rotation); float s = PXL_fast_sin(rotation);

			scaled_width -= origin_x; scaled_height -= origin_y;

			//set vertex position including scale and rotation
			v[0].pos.x = x + ((c * -origin_x) - (s * -origin_y));
			v[0].pos.y = y + ((s * -origin_x) + (c * -origin_y));

			v[1].pos.x = x + ((c * scaled_width) - (s * -origin_y));
			v[1].pos.y = y + ((s * scaled_width) + (c * -origin_y));

			v[2].pos.x = x + ((c * scaled_width) - (s * scaled_height));
			v[2].pos.y = y + ((s * scaled_width) + (c * scaled_height));

			v[3].pos.x = x + ((c * -origin_x) - (s * scaled_height));
			v[3].pos.y = y + ((s * -origin_x) + (c * scaled_height));

			v_batch->rotation = rotation;
		}else {
			//set vertex position including scale
			v[0].pos.x = x;											v[0].pos.y = y;
			v[1].pos.x = x + scaled_width;							v[1].pos.y = y;
			v[2].pos.x = x + scaled_width;							v[2].pos.y = y + scaled_height;
			v[3].pos.x = x;											v[3].pos.y = y + scaled_height;
		}

		v_batch->rect = *rect;
		v_batch->origin.x = origin_x; v_batch->origin.y = origin_y;
		v_batch->flip = flip;
	}

	/**
	==================================================================================
								Set UV vertex coords
	==================================================================================
	**/
	//attempt to optimise by not setting uv values if they have the same value in the vertex batch as the new values
	bool set_coords = false;
	if (src_rect == NULL) {
		if (v_batch->src_rect.x != 0 || v_batch->src_rect.y != 0 ||
			v_batch->src_rect.w != texture->get_width() || v_batch->src_rect.h != texture->get_height()) {
			set_coords = true;
		}
	}else {
		if (v_batch->src_rect.x != src_rect->x || v_batch->src_rect.y != src_rect->y ||
			v_batch->src_rect.w != src_rect->w || v_batch->src_rect.h != src_rect->h) {
			set_coords = true;
		}
	}

	if (set_coords) {
		//default un-normalised uv coords
		unsigned short uv_x = 0; unsigned short uv_y = 0; unsigned short uv_w = USHRT_MAX; unsigned short uv_h = USHRT_MAX;
		if (src_rect != NULL) {
			//calculate uv x, y, w, h by the src rect
			uv_x = (src_rect->x / texture->get_width()) * USHRT_MAX; uv_y = (src_rect->y / texture->get_height()) * USHRT_MAX;
			uv_w = (src_rect->w / texture->get_width()) * USHRT_MAX; uv_h = (src_rect->h / texture->get_height()) * USHRT_MAX;
		}

		//set uv coordinates
		v[0].uv.x = uv_x;										v[0].uv.y = uv_y;
		v[1].uv.x = uv_x + uv_w;								v[1].uv.y = uv_y;
		v[2].uv.x = uv_x + uv_w;								v[2].uv.y = uv_y + uv_h;
		v[3].uv.x = uv_x;										v[3].uv.y = uv_y + uv_h;

		if (src_rect == NULL) {
			v_batch->src_rect.x = 0; v_batch->src_rect.y = 0;
			v_batch->src_rect.w = texture->get_width(); v_batch->src_rect.h = texture->get_height();
		}else {
			v_batch->src_rect = *src_rect;
		}
	}

	/**
	==================================================================================
								Set vertex colours
	==================================================================================
	**/
	int i_r = r * 255; int i_g = g * 255; int i_b = b * 255; int i_a = a * 255;

	if (v_batch->colour.r != i_r || v_batch->colour.g != i_g || v_batch->colour.b != i_b || v_batch->colour.a != i_a) {
		//set vertex colours
		for (int n = 0; n < 4; ++n) {
			v[n].colour.r = i_r;
			v[n].colour.g = i_g;
			v[n].colour.b = i_b;
			v[n].colour.a = i_a;
		}
		v_batch->colour.r = i_r;
		v_batch->colour.g = i_g;
		v_batch->colour.b = i_b;
		v_batch->colour.a = i_a;
	}
}

void PXL_Batch::set_target(PXL_FrameBuffer* f) {
	//sets the target frame buffer to be used for rendering
	target_frame_buffer = f;
}

void PXL_Batch::draw_vbo() {
	//if there are no textures to draw or no vertex data then return
	if (num_added == 0) { return; }

	std::stable_sort(vertex_data, vertex_data + (num_added * 4), [](PXL_VertexPoint a, PXL_VertexPoint b) {
		return a.z_depth < b.z_depth;
	});

	//binds vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
	glBufferData(GL_ARRAY_BUFFER, num_added * 4 * sizeof(PXL_VertexPoint), &vertex_data[0], GL_DYNAMIC_DRAW);

	std::stable_sort(vertex_batches, vertex_batches + num_added, [](PXL_VertexBatch a, PXL_VertexBatch b) {
		return a.z_depth < b.z_depth;
	});

	//loops through each texture and draws the vertex data with that texture id
	int batch_index = 0;
	int offset = 0;
	int size = 0;
	bool texture_changed;
	bool changed;

	int prev_id = vertex_batches[0].texture_id;
	PXL_BlendMode prev_blend_mode = vertex_batches[0].blend_mode;
	//use_blend_mode(prev_blend_mode);
	use_blend_mode(PXL_ALPHA_BLEND);
	PXL_ShaderProgram* prev_shader = vertex_batches[0].shader;
	use_shader(prev_shader);
	for (int i = 0; i < num_added + 1; ++i) {
		texture_changed = false;
		changed = false;

		if (vertex_batches[i].texture_id != prev_id || i >= num_added - 1) {
			glBindTexture(GL_TEXTURE_2D, prev_id);
			prev_id = vertex_batches[i].texture_id;
			texture_changed = true;
			changed = true;
		}
		if (texture_changed || vertex_batches[i].shader != prev_shader) {
			use_shader(prev_shader);
			prev_shader = vertex_batches[i].shader;
			changed = true;
		}
		if (texture_changed || vertex_batches[i].blend_mode != prev_blend_mode) {
			//use_blend_mode(prev_blend_mode);
			use_blend_mode(PXL_ALPHA_BLEND);
			prev_blend_mode = vertex_batches[i].blend_mode;
			changed = true;
		}

		if (changed) {
			//draw vertex data from vertex data in buffer
			glDrawArrays(GL_QUADS, offset, size);

			offset += size;
			size = 0;
		}
		size += vertex_batches[i].num_vertices;
	}
}

void PXL_Batch::free() {
	glDeleteBuffers(1, &vertex_buffer_id);
	vbo_created = false;
}

PXL_Batch::~PXL_Batch() {
	free();
}