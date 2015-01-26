#include "PXL_FrameBuffer.h"
#include <iostream>
#include "PXL_Window.h"

PXL_FrameBuffer::PXL_FrameBuffer() {
	frame_buffer_created = false;
	create_frame_buffer();
}

PXL_FrameBuffer* PXL_create_frame_buffer() {
	return new PXL_FrameBuffer();
}

void PXL_FrameBuffer::create_frame_buffer() {
	//if the frame_buffer is already created then delete the frame_buffer but not the buffer
	if (frame_buffer_created) {
		glDeleteFramebuffers(1, &id);
		frame_buffer_created = false;
	}

	glGenFramebuffers(1, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, id);
	glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, PXL_window_width);
	glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, PXL_window_height);
	glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES, 4);

	texture = new PXL_Texture(PXL_window_width, PXL_window_height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, get_texture_id(), 0);

	clear(1, 1, 1, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "an error occurred when creating a frame buffer\n";
	}

	frame_buffer_created = true;

	//bind to default frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PXL_FrameBuffer::clear(float r, float g, float b, float a) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
	glClearColor(r, g, b, a);
	glClearBufferiv(GL_DRAW_FRAMEBUFFER, get_texture_id(), 0);
	glClear(GL_COLOR_BUFFER_BIT);
}

unsigned char* PXL_FrameBuffer::get_pixels() {
	if (frame_buffer_created) {
		glBindBuffer(GL_TEXTURE_2D, get_texture_id());
		unsigned char* pixels = new unsigned char[(width * height) * 4];
		glGetTexImage(GL_FRAMEBUFFER, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		return pixels;
	}
	return NULL;
}

void PXL_FrameBuffer::free() {
	if (frame_buffer_created) {
		glDeleteFramebuffers(1, &id);
		frame_buffer_created = false;
	}
}

PXL_FrameBuffer::~PXL_FrameBuffer() {
	free();
}