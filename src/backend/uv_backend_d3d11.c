#define ULIVO_BACKEND_DATA
#include "../ulivo.h"

#include <initguid.h>
#include <d3d11.h>

#include <string.h>
#include <assert.h>

#ifndef DONT_USE_TLOG
typedef void (*on_fatal_cb_f)(const char *msg, void *userdata);

enum {
    LogAll, LogTrace, LogDebug, LogInfo, LogWarning, LogError, LogFatal
};

static void traceLog(int level, const char *fmt, ...);
static void traceSetFatalCallback(on_fatal_cb_f cb, void *userdata);

#define tall(...)  traceLog(LogAll, __VA_ARGS__)
#define trace(...) traceLog(LogTrace, __VA_ARGS__)
#define debug(...) traceLog(LogDebug, __VA_ARGS__)
#define info(...)  traceLog(LogInfo, __VA_ARGS__)
#define warn(...)  traceLog(LogWarning, __VA_ARGS__)
#define err(...)   traceLog(LogError, __VA_ARGS__)
#define fatal(...) traceLog(LogFatal, __VA_ARGS__)
#endif

#pragma comment(lib, "d3d11.lib")

#define SAFE_RELEASE(p) if(p) { (p)->lpVtbl->Release(p); (p) = NULL; }

static void fatalCallBack(const char *msg, void *udata) {}

static void d3d11LogMessages(void);
static bool d3d11Init(void);
static void d3d11UpdateVtxBuf(uv_vertex_t *vertices, uint32_t count);
static void d3d11UpdateIdxBuf(uv_index_t *indices, uint32_t count);

static vec2i win_size = { 0, 0 };

static ID3D11Device           *device = NULL;
static ID3D11DeviceContext    *context = NULL;
static ID3D11Debug            *debugdev = NULL;
static ID3D11InfoQueue        *infodev = NULL;
static IDXGISwapChain         *swapchain = NULL;
static ID3D11RasterizerState  *rasterizer_state = NULL;
static ID3D11RenderTargetView *back_buffer_rtv = NULL;

static ID3D11Buffer *vertex_buf = NULL;
static ID3D11Buffer *index_buf = NULL;
static uint32_t vertex_count = 0;
static uint32_t index_count = 0;

static ID3D11VertexShader *vertex_shader = NULL;
static ID3D11PixelShader *pixel_shader = NULL;
static ID3D11InputLayout *input_layout = NULL;
static ID3D11SamplerState *sampler_state = NULL;
static ID3D11Buffer *vertex_cbuf = NULL;
static ID3D11Texture2D *default_texture = NULL;
static ID3D11ShaderResourceView *default_texture_srv = NULL;
static const uint8_t vs_data[1040];
static const uint8_t ps_data[744];

void uv__backend_init_gfx(void) {
	traceSetFatalCallback(fatalCallBack, NULL);

    if (!d3d11Init()) {
        d3d11LogMessages();
        fatal("couldn't init DirectX");
    }
}

void uv__backend_cleanup_gfx(void) {
#ifndef NDEBUG
		// we need this as it doesn't report memory leak otherwise
		infodev->lpVtbl->PushEmptyStorageFilter(infodev);
#endif
        SAFE_RELEASE(default_texture_srv);
        SAFE_RELEASE(default_texture);
        SAFE_RELEASE(back_buffer_rtv);
        SAFE_RELEASE(vertex_cbuf);
        SAFE_RELEASE(input_layout);
        SAFE_RELEASE(vertex_shader);
        SAFE_RELEASE(pixel_shader);
        SAFE_RELEASE(input_layout);
        SAFE_RELEASE(sampler_state);
        SAFE_RELEASE(vertex_cbuf);

		// SAFE_RELEASE(depth_stencil_state);
		SAFE_RELEASE(rasterizer_state);
		SAFE_RELEASE(swapchain);
		SAFE_RELEASE(context);
		SAFE_RELEASE(device);
#ifndef NDEBUG
		SAFE_RELEASE(infodev);
		debugdev->lpVtbl->ReportLiveDeviceObjects(debugdev, D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		SAFE_RELEASE(debugdev);
#endif
}

void uv__backend_draw(colour_t clear_colour, uv_drawdata_t *data) {
	if (!data) return;

	// update projection matrix
	
	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	HRESULT hr = context->lpVtbl->Map(context, (ID3D11Resource *)vertex_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
	if (FAILED(hr)) {
		err("couldn't bind vertex constant buffer");
		return;
	}

	float l = 0.f;               // left
	float r = (float)win_size.x; // right
	float t = 0.f;               // top
	float b = (float)win_size.y; // bottom
	float n = 0.f;               // z near
	float f = 100.f;             // z far

	float proj_mat[16] = {
		2.f / (r - l), 0,             0,             (l + r) / (l - r),
		0,             2.f / (t - b), 0,             (b + t) / (b - t),
		0,             0,             2.f / (n - f), (f + n) / (n - f),
		0,             0,             0,             1
	};

    memcpy(mapped_resource.pData, proj_mat, sizeof(proj_mat));

	context->lpVtbl->Unmap(context, (ID3D11Resource *)vertex_cbuf, 0);

    // update vertex and index buffers
    
    d3d11UpdateVtxBuf(data->vertices, data->vtx_count);
    d3d11UpdateIdxBuf(data->indices, data->idx_count);

    // setup

	D3D11_VIEWPORT viewport = {
		.Width    = (float)win_size.x,
		.Height   = (float)win_size.y,
		.TopLeftX = 0.f,
		.TopLeftY = 0.f,
		.MinDepth = 0.f,
		.MaxDepth = 1.f,
	};

	context->lpVtbl->RSSetViewports(context, 1, &viewport);
	context->lpVtbl->RSSetState(context, rasterizer_state);
	context->lpVtbl->OMSetRenderTargets(context,  1, &back_buffer_rtv, NULL);

    context->lpVtbl->VSSetShader(context, vertex_shader, NULL, 0);
    context->lpVtbl->PSSetShader(context, pixel_shader,  NULL, 0);

    context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &vertex_cbuf);
    context->lpVtbl->PSSetSamplers(context, 0, 1, &sampler_state);

    UINT vtx_stride = sizeof(uv_vertex_t);
    UINT vtx_offset = 0;

	context->lpVtbl->IASetInputLayout(context, input_layout);
	context->lpVtbl->IASetVertexBuffers(context, 0, 1, &vertex_buf, &vtx_stride, &vtx_offset);
    context->lpVtbl->IASetIndexBuffer(context, index_buf, DXGI_FORMAT_R32_UINT, 0);
    context->lpVtbl->IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->lpVtbl->ClearRenderTargetView(context, back_buffer_rtv, (float*)&clear_colour);

    // draw each batch
    for (uint32_t i = 0; i < data->batch_count; ++i) {
        uv_batch_t *batch = &data->batches[i];
        ID3D11ShaderResourceView *texture = (ID3D11ShaderResourceView *)batch->texture;
        if (!texture) texture = default_texture_srv;

        context->lpVtbl->PSSetShaderResources(context, 0, 1, &texture);
        context->lpVtbl->DrawIndexed(context, batch->idx_count, batch->idx_start, 0);
    }

    // cleanup
    ID3D11ShaderResourceView *null_srv = NULL;
    context->lpVtbl->PSSetShaderResources(context, 0, 1, &null_srv);
    context->lpVtbl->VSSetShader(context, NULL, NULL, 0);
    context->lpVtbl->PSSetShader(context, NULL, NULL, 0);

	swapchain->lpVtbl->Present(swapchain, 1, 0);
}

void uv__backend_resize_gfx(int new_width, int new_height) {
    win_size = (vec2i){ new_width, new_height };
	
	if (swapchain) {
		//swapchain->lpVtbl->ResizeBuffers(swapchain, 0, (UINT)new_width, (UINT)new_height, DXGI_FORMAT_UNKNOWN, 0);
	}
}

// == STATIC FUNCTIONS ========================================================

static bool d3d11Init(void) {
    // -- create device, context, and swap chain --

    DXGI_SWAP_CHAIN_DESC sd = {
        .BufferCount = 2,
        .BufferDesc = {
            .Width  = (UINT)win_size.x,
            .Height = (UINT)win_size.y,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .RefreshRate.Numerator = 60,
            .RefreshRate.Denominator = 1,
        },
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .OutputWindow = GetActiveWindow(),
        .SampleDesc = {
            .Count = 1,
            .Quality = 0
        },
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD
    };

    UINT createDeviceFlags = 0;
#ifndef NDEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapchain,
        &device,
        &featureLevel,
        &context
    );
    if (FAILED(hr)) {
        err("couldn't create device");
        return false;
    }

    // -- setup debug logging --

#ifndef NDEBUG
    
    hr = device->lpVtbl->QueryInterface(device, &IID_ID3D11Debug, &debugdev);
    assert(SUCCEEDED(hr));
    hr = device->lpVtbl->QueryInterface(device, &IID_ID3D11InfoQueue, &infodev);
    assert(SUCCEEDED(hr));
#endif

	// -- create rasterizer state --

	D3D11_RASTERIZER_DESC rast_desc = {
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_BACK,
		.FrontCounterClockwise = TRUE
	};
	device->lpVtbl->CreateRasterizerState(device, &rast_desc, &rasterizer_state);

    // -- create shaders --

    hr = device->lpVtbl->CreateVertexShader(device, vs_data, sizeof(vs_data), NULL, &vertex_shader);
    if (FAILED(hr)) {
        err("couldn't create vertex shader: %d", hr);
        return false;
    }

    hr = device->lpVtbl->CreatePixelShader(device, ps_data, sizeof(ps_data), NULL, &pixel_shader);
    if (FAILED(hr)) {
        err("couldn't create pixel shader");
        return false;
    }

    // -- create input layout --

    D3D11_INPUT_ELEMENT_DESC in_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->lpVtbl->CreateInputLayout(device, in_layout, sizeof(in_layout)/sizeof(*in_layout), vs_data, sizeof(vs_data), &input_layout);
    if (FAILED(hr)) {
        err("couldn't create input layout");
        return false;
    }
    // -- create texture sampler --

    D3D11_SAMPLER_DESC sampler_desc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP
    };

    hr = device->lpVtbl->CreateSamplerState(device, &sampler_desc, &sampler_state);
    if (FAILED(hr)) {
        err("couldn't create sampler state");
        return false;
    }

    // -- create constant buffer --

    D3D11_BUFFER_DESC cbuf_desc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(float) * 16,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    hr = device->lpVtbl->CreateBuffer(device, &cbuf_desc, NULL, &vertex_cbuf);
    if (FAILED(hr)) {
        err("couldn't create vertex's constant buffer");
        return false;
    }

	// -- create render target view --

	ID3D11Texture2D *back_buffer = NULL;
	hr = swapchain->lpVtbl->GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void**)(&back_buffer)); 
	if (FAILED(hr)) {
		err("failed to get the backbuffer");
		return false;
	}

	hr = device->lpVtbl->CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, NULL, &back_buffer_rtv); 
	if (FAILED(hr)) {
		err("failed to get render target view of the backbuffer");
		return false;
	}

	SAFE_RELEASE(back_buffer);

	D3D11_TEXTURE2D_DESC default_desc = {
		.Width            = 1,
		.Height           = 1,
		.MipLevels        = 1,
		.ArraySize        = 1,
		.Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Usage            = D3D11_USAGE_IMMUTABLE,
		.BindFlags        = D3D11_BIND_SHADER_RESOURCE,
		.SampleDesc.Count = 1,
	};
	
    uint32_t default_pixel = 0xffffffff;

    D3D11_SUBRESOURCE_DATA default_data = { 
		.pSysMem = &default_pixel, 
		.SysMemPitch = sizeof(default_pixel) 
	};

	hr = device->lpVtbl->CreateTexture2D(device, &default_desc, &default_data, &default_texture);
	if (FAILED(hr)) {
		err("failed to create default white texture");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
		.Format = default_desc.Format,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D.MipLevels = 1,
	};

	hr = device->lpVtbl->CreateShaderResourceView(device, (ID3D11Resource*)default_texture, &srv_desc, &default_texture_srv);
	if (FAILED(hr)) {
		err("failed to create default shader resource view");
		return false;
	}

    return true;
}

texture_t uv__backend_load_texture(const image_t *image) {
	if (!image || !image->data) return 0;

	ID3D11Texture2D *texture = NULL;
	ID3D11ShaderResourceView *srv = NULL;

	D3D11_TEXTURE2D_DESC tex_desc = {
		.Width            = image->width,
		.Height           = image->height,
		.MipLevels        = 1,
		.ArraySize        = 1,
		.Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
		//.Usage            = D3D11_USAGE_IMMUTABLE,
		.Usage            = D3D11_USAGE_DEFAULT,
		.BindFlags        = D3D11_BIND_SHADER_RESOURCE,
		.SampleDesc.Count = 1,
	};
	

    D3D11_SUBRESOURCE_DATA data_desc = { 
		.pSysMem = image->data, 
		.SysMemPitch = image->width * 4
	};

	HRESULT hr = device->lpVtbl->CreateTexture2D(device, &tex_desc, &data_desc, &texture);
	// HRESULT hr = device->lpVtbl->CreateTexture2D(device, &tex_desc, NULL, &texture);
	if (FAILED(hr)) {
		err("failed to create texture");
		return 0;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
		.Format = tex_desc.Format,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D.MipLevels = 1,
	};

	hr = device->lpVtbl->CreateShaderResourceView(device, (ID3D11Resource*)texture, &srv_desc, &srv);
	if (FAILED(hr)) {
		err("failed to create texture's shader resource view");
		return 0;
	}

	return (uintptr_t)srv;
}

void uv__backend_free_texture(texture_t texture) {
	// TODO
}

// == STATIC FUNCTIONS ========================================================

static void d3d11LogMessages(void) {
    UINT64 message_count = infodev->lpVtbl->GetNumStoredMessages(infodev);

    D3D11_MESSAGE* msg = NULL;
    SIZE_T old_size = 0;

    for (UINT64 i = 0; i < message_count; ++i) {
        SIZE_T msg_size = 0;
        infodev->lpVtbl->GetMessage(infodev, i, NULL, &msg_size);
        
        if (msg_size > old_size) {
            D3D11_MESSAGE *new_msg = realloc(msg, msg_size);
            if (!new_msg) {
                fatal("couldn't reallocate message");
            }
            msg = new_msg;
        }
        infodev->lpVtbl->GetMessage(infodev, i, msg, &msg_size);
        assert(msg);
        
        switch (msg->Severity) {
        case D3D11_MESSAGE_SEVERITY_CORRUPTION: fatal("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
        case D3D11_MESSAGE_SEVERITY_ERROR:      err("%.*s", msg->DescriptionByteLength, msg->pDescription);   break;
        case D3D11_MESSAGE_SEVERITY_WARNING:    warn("%.*s", msg->DescriptionByteLength, msg->pDescription);  break;
        case D3D11_MESSAGE_SEVERITY_INFO:       info("%.*s", msg->DescriptionByteLength, msg->pDescription);  break;
        case D3D11_MESSAGE_SEVERITY_MESSAGE:    tall("%.*s", msg->DescriptionByteLength, msg->pDescription);  break;
        }
    }

    free(msg);
    infodev->lpVtbl->ClearStoredMessages(infodev);
}

static void d3d11UpdateVtxBuf(uv_vertex_t *vertices, uint32_t count) {
    if (!vertex_buf || vertex_count < count) {
        SAFE_RELEASE(vertex_buf);
        vertex_count = count;

        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(uv_vertex_t) * count,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
        };

        HRESULT hr = E_FAIL;

        if (vertices) {
            D3D11_SUBRESOURCE_DATA init_data = {
                .pSysMem = vertices
            };
            hr = device->lpVtbl->CreateBuffer(device, &desc, &init_data, &vertex_buf);
        }
        else {
            hr = device->lpVtbl->CreateBuffer(device, &desc, NULL, &vertex_buf);
        }

        if (FAILED(hr)) {
            fatal("couldn't update vertex buffer");
        }
    }
    else {
        D3D11_MAPPED_SUBRESOURCE mapped = {0};
        HRESULT hr = context->lpVtbl->Map(context, (ID3D11Resource *)vertex_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, vertices, sizeof(uv_vertex_t) * count);
            context->lpVtbl->Unmap(context, (ID3D11Resource *)vertex_buf, 0);
        }
        else {
            fatal("couldn't map vertex buffer");
        }
    }
}

static void d3d11UpdateIdxBuf(uv_index_t *indices, uint32_t count) {
    if (!index_buf || index_count < count) {
        SAFE_RELEASE(index_buf);
        index_count = count;

        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(uv_index_t) * count,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
        };

        D3D11_SUBRESOURCE_DATA init_data = {
            .pSysMem = indices
        };

        HRESULT hr = device->lpVtbl->CreateBuffer(device, &desc, &init_data, &index_buf);
        if (FAILED(hr)) {
            fatal("couldn't update index buffer");
        }
    }
    else {
        D3D11_MAPPED_SUBRESOURCE mapped = {0};
        HRESULT hr = context->lpVtbl->Map(context, (ID3D11Resource *)index_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, indices, sizeof(uv_index_t) * count);
            context->lpVtbl->Unmap(context, (ID3D11Resource *)index_buf, 0);
        }
        else {
            fatal("couldn't map index buffer");
        }
    }
}

/***********************************************************
struct VertexInput {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

struct PixelInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

cbuffer MatrixBuffer : register(b0) {
    matrix proj;
};

PixelInput VS(VertexInput input) {
    PixelInput output;
    output.pos = mul(float4(input.pos, 0.0, 1.0), proj);
    output.uv  = input.uv;
    output.col = input.col;
    return output;
}
***********************************************************/
static const u8 vs_data[] = { 
	0x44, 0x58, 0x42, 0x43,   0xc9, 0x70, 0xb4, 0xd3,   
	0x14, 0xc4, 0xef, 0x37,   0xc8, 0xc0, 0xdf, 0xa3,   
	0x97, 0x65, 0xb7, 0xc6,   0x01, 0x00, 0x00, 0x00,   
	0x10, 0x04, 0x00, 0x00,   0x05, 0x00, 0x00, 0x00,   
	0x34, 0x00, 0x00, 0x00,   0x44, 0x01, 0x00, 0x00,   
	0xb4, 0x01, 0x00, 0x00,   0x28, 0x02, 0x00, 0x00,   
	0x74, 0x03, 0x00, 0x00,   0x52, 0x44, 0x45, 0x46,   
	0x08, 0x01, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x6c, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x3c, 0x00, 0x00, 0x00,   0x00, 0x05, 0xfe, 0xff,   
	0x00, 0x01, 0x00, 0x00,   0xe0, 0x00, 0x00, 0x00,   
	0x52, 0x44, 0x31, 0x31,   0x3c, 0x00, 0x00, 0x00,   
	0x18, 0x00, 0x00, 0x00,   0x20, 0x00, 0x00, 0x00,   
	0x28, 0x00, 0x00, 0x00,   0x24, 0x00, 0x00, 0x00,   
	0x0c, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x5c, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x4d, 0x61, 0x74, 0x72,   0x69, 0x78, 0x42, 0x75,   
	0x66, 0x66, 0x65, 0x72,   0x00, 0xab, 0xab, 0xab,   
	0x5c, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x84, 0x00, 0x00, 0x00,   0x40, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0xac, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x40, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0xbc, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0xff, 0xff, 0xff, 0xff,   0x00, 0x00, 0x00, 0x00,   
	0xff, 0xff, 0xff, 0xff,   0x00, 0x00, 0x00, 0x00,   
	0x70, 0x72, 0x6f, 0x6a,   0x00, 0x66, 0x6c, 0x6f,   
	0x61, 0x74, 0x34, 0x78,   0x34, 0x00, 0xab, 0xab,   
	0x03, 0x00, 0x03, 0x00,   0x04, 0x00, 0x04, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0xb1, 0x00, 0x00, 0x00,   0x4d, 0x69, 0x63, 0x72,   
	0x6f, 0x73, 0x6f, 0x66,   0x74, 0x20, 0x28, 0x52,   
	0x29, 0x20, 0x48, 0x4c,   0x53, 0x4c, 0x20, 0x53,   
	0x68, 0x61, 0x64, 0x65,   0x72, 0x20, 0x43, 0x6f,   
	0x6d, 0x70, 0x69, 0x6c,   0x65, 0x72, 0x20, 0x31,   
	0x30, 0x2e, 0x31, 0x00,   0x49, 0x53, 0x47, 0x4e,   
	0x68, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x08, 0x00, 0x00, 0x00,   0x50, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x03, 0x00, 0x00,   0x59, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x03, 0x03, 0x00, 0x00,   0x62, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x0f, 0x0f, 0x00, 0x00,   0x50, 0x4f, 0x53, 0x49,   
	0x54, 0x49, 0x4f, 0x4e,   0x00, 0x54, 0x45, 0x58,   
	0x43, 0x4f, 0x4f, 0x52,   0x44, 0x00, 0x43, 0x4f,   
	0x4c, 0x4f, 0x52, 0x00,   0x4f, 0x53, 0x47, 0x4e,   
	0x6c, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x08, 0x00, 0x00, 0x00,   0x50, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x0f, 0x00, 0x00, 0x00,   0x5c, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x03, 0x0c, 0x00, 0x00,   0x65, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x0f, 0x00, 0x00, 0x00,   0x53, 0x56, 0x5f, 0x50,   
	0x4f, 0x53, 0x49, 0x54,   0x49, 0x4f, 0x4e, 0x00,   
	0x54, 0x45, 0x58, 0x43,   0x4f, 0x4f, 0x52, 0x44,   
	0x00, 0x43, 0x4f, 0x4c,   0x4f, 0x52, 0x00, 0xab,   
	0x53, 0x48, 0x45, 0x58,   0x44, 0x01, 0x00, 0x00,   
	0x50, 0x00, 0x01, 0x00,   0x51, 0x00, 0x00, 0x00,   
	0x6a, 0x08, 0x00, 0x01,   0x59, 0x00, 0x00, 0x04,   
	0x46, 0x8e, 0x20, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x04, 0x00, 0x00, 0x00,   0x5f, 0x00, 0x00, 0x03,   
	0x32, 0x10, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x5f, 0x00, 0x00, 0x03,   0x32, 0x10, 0x10, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x5f, 0x00, 0x00, 0x03,   
	0xf2, 0x10, 0x10, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x67, 0x00, 0x00, 0x04,   0xf2, 0x20, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x65, 0x00, 0x00, 0x03,   0x32, 0x20, 0x10, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x65, 0x00, 0x00, 0x03,   
	0xf2, 0x20, 0x10, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x68, 0x00, 0x00, 0x02,   0x01, 0x00, 0x00, 0x00,   
	0x36, 0x00, 0x00, 0x05,   0x32, 0x00, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x10, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x36, 0x00, 0x00, 0x05,   
	0x42, 0x00, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x40, 0x00, 0x00,   0x00, 0x00, 0x80, 0x3f,   
	0x10, 0x00, 0x00, 0x08,   0x12, 0x20, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x02, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x83, 0x20, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x10, 0x00, 0x00, 0x08,   0x22, 0x20, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x02, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x83, 0x20, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x10, 0x00, 0x00, 0x08,   0x42, 0x20, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x02, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x83, 0x20, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x10, 0x00, 0x00, 0x08,   0x82, 0x20, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x02, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x83, 0x20, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x36, 0x00, 0x00, 0x05,   0x32, 0x20, 0x10, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x46, 0x10, 0x10, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x36, 0x00, 0x00, 0x05,   
	0xf2, 0x20, 0x10, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x46, 0x1e, 0x10, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x3e, 0x00, 0x00, 0x01,   0x53, 0x54, 0x41, 0x54,   
	0x94, 0x00, 0x00, 0x00,   0x09, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x06, 0x00, 0x00, 0x00,   0x04, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x04, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
};


/***********************************************************
struct PixelInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

Texture2D diffuse_texture;
SamplerState Sampler0;

float4 PS(PixelInput input) : SV_Target {
    float4 colour = input.col * diffuse_texture.Sample(Sampler0, input.uv);
    return colour;
}
***********************************************************/
static const u8 ps_data[] ={ 
	0x44, 0x58, 0x42, 0x43,   0xa1, 0xb7, 0x7f, 0x07,   
	0xb0, 0xf3, 0x81, 0xac,   0xa5, 0x4a, 0xa8, 0x9e,   
	0x08, 0x29, 0x16, 0x1f,   0x01, 0x00, 0x00, 0x00,   
	0xe8, 0x02, 0x00, 0x00,   0x05, 0x00, 0x00, 0x00,   
	0x34, 0x00, 0x00, 0x00,   0xfc, 0x00, 0x00, 0x00,   
	0x70, 0x01, 0x00, 0x00,   0xa4, 0x01, 0x00, 0x00,   
	0x4c, 0x02, 0x00, 0x00,   0x52, 0x44, 0x45, 0x46,   
	0xc0, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x3c, 0x00, 0x00, 0x00,   0x00, 0x05, 0xff, 0xff,   
	0x00, 0x01, 0x00, 0x00,   0x95, 0x00, 0x00, 0x00,   
	0x52, 0x44, 0x31, 0x31,   0x3c, 0x00, 0x00, 0x00,   
	0x18, 0x00, 0x00, 0x00,   0x20, 0x00, 0x00, 0x00,   
	0x28, 0x00, 0x00, 0x00,   0x24, 0x00, 0x00, 0x00,   
	0x0c, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x7c, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x85, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x05, 0x00, 0x00, 0x00,   0x04, 0x00, 0x00, 0x00,   
	0xff, 0xff, 0xff, 0xff,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x0c, 0x00, 0x00, 0x00,   
	0x53, 0x61, 0x6d, 0x70,   0x6c, 0x65, 0x72, 0x30,   
	0x00, 0x64, 0x69, 0x66,   0x66, 0x75, 0x73, 0x65,   
	0x5f, 0x74, 0x65, 0x78,   0x74, 0x75, 0x72, 0x65,   
	0x00, 0x4d, 0x69, 0x63,   0x72, 0x6f, 0x73, 0x6f,   
	0x66, 0x74, 0x20, 0x28,   0x52, 0x29, 0x20, 0x48,   
	0x4c, 0x53, 0x4c, 0x20,   0x53, 0x68, 0x61, 0x64,   
	0x65, 0x72, 0x20, 0x43,   0x6f, 0x6d, 0x70, 0x69,   
	0x6c, 0x65, 0x72, 0x20,   0x31, 0x30, 0x2e, 0x31,   
	0x00, 0xab, 0xab, 0xab,   0x49, 0x53, 0x47, 0x4e,   
	0x6c, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x08, 0x00, 0x00, 0x00,   0x50, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x0f, 0x00, 0x00, 0x00,   0x5c, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x03, 0x03, 0x00, 0x00,   0x65, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x0f, 0x0f, 0x00, 0x00,   0x53, 0x56, 0x5f, 0x50,   
	0x4f, 0x53, 0x49, 0x54,   0x49, 0x4f, 0x4e, 0x00,   
	0x54, 0x45, 0x58, 0x43,   0x4f, 0x4f, 0x52, 0x44,   
	0x00, 0x43, 0x4f, 0x4c,   0x4f, 0x52, 0x00, 0xab,   
	0x4f, 0x53, 0x47, 0x4e,   0x2c, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x08, 0x00, 0x00, 0x00,   
	0x20, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x0f, 0x00, 0x00, 0x00,   
	0x53, 0x56, 0x5f, 0x54,   0x61, 0x72, 0x67, 0x65,   
	0x74, 0x00, 0xab, 0xab,   0x53, 0x48, 0x45, 0x58,   
	0xa0, 0x00, 0x00, 0x00,   0x50, 0x00, 0x00, 0x00,   
	0x28, 0x00, 0x00, 0x00,   0x6a, 0x08, 0x00, 0x01,   
	0x5a, 0x00, 0x00, 0x03,   0x00, 0x60, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x58, 0x18, 0x00, 0x04,   
	0x00, 0x70, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x55, 0x55, 0x00, 0x00,   0x62, 0x10, 0x00, 0x03,   
	0x32, 0x10, 0x10, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x62, 0x10, 0x00, 0x03,   0xf2, 0x10, 0x10, 0x00,   
	0x02, 0x00, 0x00, 0x00,   0x65, 0x00, 0x00, 0x03,   
	0xf2, 0x20, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x68, 0x00, 0x00, 0x02,   0x01, 0x00, 0x00, 0x00,   
	0x45, 0x00, 0x00, 0x8b,   0xc2, 0x00, 0x00, 0x80,   
	0x43, 0x55, 0x15, 0x00,   0xf2, 0x00, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x46, 0x10, 0x10, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x46, 0x7e, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x60, 0x10, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x38, 0x00, 0x00, 0x07,   
	0xf2, 0x20, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x46, 0x0e, 0x10, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x46, 0x1e, 0x10, 0x00,   0x02, 0x00, 0x00, 0x00,   
	0x3e, 0x00, 0x00, 0x01,   0x53, 0x54, 0x41, 0x54,   
	0x94, 0x00, 0x00, 0x00,   0x03, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x03, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   
};

#ifndef DONT_USE_TLOG

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS.
#include <Windows.h>

#define MAX_TRACELOG_MSG_LENGTH 1024

static on_fatal_cb_f fatal_callback = NULL;
static void *fatal_cb_userdata = NULL;

static void setLevelColour(int level) {
    WORD attribute = 15;
    switch (level) {
        case LogDebug:   attribute = 1; break; 
        case LogInfo:    attribute = 2; break;
        case LogWarning: attribute = 6; break;
        case LogError:   attribute = 4; break;
        case LogFatal:   attribute = 4; break;
    }

    HANDLE hc = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hc, attribute);
}

static void traceLogVaList(int level, const char *fmt, va_list args) {
	    char buffer[MAX_TRACELOG_MSG_LENGTH];
    memset(buffer, 0, sizeof(buffer));

    const char *beg;
    switch (level) {
        case LogTrace:   beg = "[TRACE]: ";   break; 
        case LogDebug:   beg = "[DEBUG]: ";   break; 
        case LogInfo:    beg = "[INFO]: ";    break; 
        case LogWarning: beg = "[WARNING]: "; break; 
        case LogError:   beg = "[ERROR]: ";   break; 
        case LogFatal:   beg = "[FATAL]: ";   break;        
        default:         beg = "";                              break;
    }

    size_t offset = 0;

    vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);

    SetConsoleOutputCP(CP_UTF8);
    setLevelColour(level);
    printf("%s", beg);
    // set back to white
    setLevelColour(LogTrace);
    printf("%s\n", buffer);

    if (level == LogFatal) {
        if (fatal_callback) {
            fatal_callback(buffer + offset, fatal_cb_userdata);
        }
#ifndef TLOG_DONT_EXIT_ON_FATAL
        exit(1);
#endif
    }

#ifndef TLOG_DONT_EXIT_ON_FATAL
    if (level == LogFatal) exit(1);
#endif

}

static void traceLog(int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    traceLogVaList(level, fmt, args);
    va_end(args);
}

static void traceSetFatalCallback(on_fatal_cb_f cb, void *userdata) {
	fatal_callback = cb;
    fatal_cb_userdata = userdata;
}

#endif